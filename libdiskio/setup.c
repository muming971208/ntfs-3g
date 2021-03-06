/**
 * Copyright (c) 2014-2016 Fredrik Wikstrom <fredrik@a500.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "diskio_internal.h"
#include <dos/filehandler.h>

struct DiskIO *DIO_Setup(CONST_STRPTR name, const struct TagItem *tags) {
	struct DiskIO *dio = NULL;
	struct TagItem *tstate;
	const struct TagItem *tag;
#ifndef DISABLE_DOSTYPE_CHECK
	ULONG check_dostype = 0;
	ULONG dostype_mask = ~0;
#endif
	struct DosList *dol;
	struct DeviceNode *dn = NULL;
	struct FileSysStartupMsg *fssm = NULL;
	struct DosEnvec *de = NULL;
	struct MsgPort *mp = NULL;
	struct IOExtTD *iotd = NULL;
	char devname[256];
	struct NSDeviceQueryResult nsdqr;
	int error = DIO_ERROR_UNSPECIFIED;
	int *error_storage;

	DEBUGF("DIO_Setup('%s', %#p)\n", name, tags);

	if (name == NULL || name[0] == '\0' || name[0] == ':')
		goto cleanup;

	dio = AllocMem(sizeof(*dio), MEMF_PUBLIC|MEMF_CLEAR);
	if (dio == NULL) {
		error = DIO_ERROR_NOMEM;
		goto cleanup;
	}

	/* Enable caches by default */
	dio->cache_enabled       = TRUE;
	dio->write_cache_enabled = TRUE;

	tstate = (struct TagItem *)tags;
	while ((tag = NextTagItem(&tstate)) != NULL) {
		switch (tag->ti_Tag) {
			case DIOS_Cache:
				dio->cache_enabled = !!tag->ti_Data;
				break;
			case DIOS_WriteCache:
				dio->write_cache_enabled = !!tag->ti_Data;
				break;
			case DIOS_Inhibit:
				dio->inhibit = !!tag->ti_Data;
				break;
#ifndef DISABLE_DOSTYPE_CHECK
			case DIOS_DOSType:
				check_dostype = tag->ti_Data;
				break;
			case DIOS_DOSTypeMask:
				dostype_mask = tag->ti_Data;
				break;
#endif
			case DIOS_ReadOnly:
				dio->read_only = !!tag->ti_Data;
				break;
		}
	}

	/* If read-only mode is enabled, disable write cache */
	if (dio->read_only)
		dio->write_cache_enabled = FALSE;

	/* If cache is disabled, write cache should be disabled too */
	if (dio->cache_enabled == FALSE)
		dio->write_cache_enabled = FALSE;

	/* Remove possible colon from name and anything else that might follow it */
	SplitName(name, ':', dio->devname, 0, sizeof(dio->devname));

	/* Find device node */
	dol = LockDosList(LDF_DEVICES|LDF_READ);
	if (dol != NULL) {
		dn = (struct DeviceNode *)FindDosEntry(dol, dio->devname, LDF_DEVICES|LDF_READ);
		UnLockDosList(LDF_DEVICES|LDF_READ);
	}
	if (dn == NULL) {
		error = DIO_ERROR_GETFSD;
		goto cleanup;
	}

	/* Add back trailing colon for Inhibit() */
	strlcat((char *)dio->devname, ":", sizeof(dio->devname));

	/* Check that device node has the necessary data */
	if ((fssm = BADDR(dn->dn_Startup)) == NULL ||
		(de = BADDR(fssm->fssm_Environ)) == NULL ||
		de->de_TableSize < DE_UPPERCYL)
	{
		error = DIO_ERROR_GETFSD;
		goto cleanup;
	}

	if (dio->inhibit) {
		if (!Inhibit(dio->devname, TRUE)) {
			error = DIO_ERROR_INHIBIT;
			goto cleanup;
		}

		dio->uninhibit = TRUE; /* So Cleanup() knows that it should uninhibit */
	}

	dio->diskmp = mp = CreateMsgPort();
	dio->diskiotd = iotd = CreateIORequest(dio->diskmp, sizeof(*iotd));
	if (iotd == NULL) {
		error = DIO_ERROR_NOMEM;
		goto cleanup;
	}

	FbxCopyStringBSTRToC(fssm->fssm_Device, (STRPTR)devname, sizeof(devname));
	if (OpenDevice((CONST_STRPTR)devname, fssm->fssm_Unit, (struct IORequest *)iotd, fssm->fssm_Flags) != 0) {
		iotd->iotd_Req.io_Device = NULL;
		error = DIO_ERROR_OPENDEVICE;
		goto cleanup;
	}

	if (de->de_LowCyl == 0) {
		dio->use_full_disk = TRUE;
	} else {
		ULONG sector_size;
		UQUAD cylinder_size;

		sector_size = de->de_SizeBlock << 2;

		if (!IsValidSectorSize(sector_size)) goto cleanup;

		SetSectorSize(dio, sector_size);

		cylinder_size = ((UQUAD)de->de_BlocksPerTrack * (UQUAD)de->de_Surfaces) << dio->sector_shift;

		dio->use_full_disk   = FALSE;
		dio->partition_start = (UQUAD)de->de_LowCyl * cylinder_size;
		dio->partition_size  = (UQUAD)(de->de_HighCyl - de->de_LowCyl + 1) * cylinder_size;
		dio->total_sectors   = dio->partition_size >> dio->sector_shift;
	}

	nsdqr.DevQueryFormat = 0;
	nsdqr.SizeAvailable  = 0;

	iotd->iotd_Req.io_Command = NSCMD_DEVICEQUERY;
	iotd->iotd_Req.io_Data    = &nsdqr;
	iotd->iotd_Req.io_Length  = sizeof(nsdqr);
	if (DoIO((struct IORequest *)iotd) == 0) {
		if (iotd->iotd_Req.io_Actual < 16 ||
			iotd->iotd_Req.io_Actual != nsdqr.SizeAvailable ||
			iotd->iotd_Req.io_Actual > sizeof(nsdqr))
		{
			error = DIO_ERROR_NSDQUERY;
			goto cleanup;
		}

		if (nsdqr.DeviceType != NSDEVTYPE_TRACKDISK) {
			error = DIO_ERROR_NSDQUERY;
			goto cleanup;
		}

		if (nsdqr.SupportedCommands != NULL) {
			UWORD cmd;
			int i = 0;
			while ((cmd = nsdqr.SupportedCommands[i++]) != CMD_INVALID) {
				if (cmd == CMD_READ)
					dio->cmd_support |= CMDSF_TD32;
				else if (cmd == ETD_READ)
					dio->cmd_support |= CMDSF_ETD32;
				else if (cmd == TD_READ64)
					dio->cmd_support |= CMDSF_TD64;
				else if (cmd == NSCMD_TD_READ64)
					dio->cmd_support |= CMDSF_NSD_TD64;
				else if (cmd == NSCMD_ETD_READ64)
					dio->cmd_support |= CMDSF_NSD_ETD64;
				else if (cmd == CMD_UPDATE)
					dio->cmd_support |= CMDSF_CMD_UPDATE;
				else if (cmd == ETD_UPDATE)
					dio->cmd_support |= CMDSF_ETD_UPDATE;
			}
		}
	} else if (iotd->iotd_Req.io_Error == IOERR_NOCMD) {
		dio->cmd_support = CMDSF_TD32|CMDSF_CMD_UPDATE;

		iotd->iotd_Req.io_Command = TD_READ64;
		iotd->iotd_Req.io_Data = NULL;
		iotd->iotd_Req.io_Actual = 0;
		iotd->iotd_Req.io_Offset = 0;
		iotd->iotd_Req.io_Length = 0;
		if (DoIO((struct IORequest *)iotd) != IOERR_NOCMD)
			dio->cmd_support |= CMDSF_TD64;
	} else {
		error = DIO_ERROR_NSDQUERY;
		goto cleanup;
	}

	if ((dio->cmd_support & (CMDSF_TD32|CMDSF_ETD32|CMDSF_TD64|CMDSF_NSD_TD64|CMDSF_NSD_ETD64)) == 0) {
		error = DIO_ERROR_NSDQUERY;
		goto cleanup;
	}

	if (dio->cmd_support & CMDSF_ETD_UPDATE)
		dio->update_cmd = ETD_UPDATE;
	else if (dio->cmd_support & CMDSF_CMD_UPDATE)
		dio->update_cmd = CMD_UPDATE;
	else
		dio->update_cmd = CMD_INVALID;

	dio->mempool = CreatePool(MEMF_PUBLIC, 4096, 1024);
	if (dio->mempool == NULL) {
		error = DIO_ERROR_NOMEM;
		goto cleanup;
	}

	Internal_Update(dio, TRUE);

	DEBUGF("DIO_Setup: %#p\n", dio);
	return dio;

cleanup:
	if (dio != NULL) DIO_Cleanup(dio);

	error_storage = (int *)GetTagData(DIOS_Error, (Tag)NULL, tags);
	if (error_storage != NULL) *error_storage = error;

	DEBUGF("DIO_Setup failed (error: %d)\n", error);
	return NULL;
}

#ifndef __AROS__
struct DiskIO *DIO_SetupTags(CONST_STRPTR name, Tag tag1, ...) {
	return DIO_Setup(name, (const struct TagItem *)&tag1);
}
#endif


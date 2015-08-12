/**
 * Copyright (c) 2015 Fredrik Wikstrom <fredrik@a500.org>
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

void DIO_Cleanup(struct DiskIO *dio) {
	DEBUGF("DIO_Cleanup(%#p)\n", dio);

	if (dio != NULL) {
		DIO_FlushIOCache(dio);

#ifndef DISABLE_BLOCK_CACHE
		if (dio->block_cache != NULL)
			CleanupBlockCache(dio->block_cache);
#endif

		DeletePool(dio->mempool);

		if (dio->disk_device != NULL)
			CloseDevice((struct IORequest *)dio->diskiotd);

		DeleteIORequest((struct IORequest *)dio->diskiotd);
		DeleteMsgPort(dio->diskmp);

		if (dio->uninhibit)
			Inhibit(dio->devname, FALSE);

		FreeMem(dio, sizeof(*dio));
	}
}

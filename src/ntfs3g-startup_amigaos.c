/**
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
 * along with this program (in the main directory of the NTFS-3G
 * distribution in the file COPYING); if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <dos/dosextens.h>
#include <proto/exec.h>
#include <proto/filesysbox.h>
#include <SDI/SDI_compiler.h>
#include "ntfs3g-handler_rev.h"

static const char USED verstag[] = VERSTAG;

#define MIN_OS_VERSION    39
#define MIN_AROSC_VERSION 41
#define MIN_FBX_VERSION   53

struct ExecBase *SysBase;
struct DOSBase *DOSBase;
struct UtilityBase *UtilityBase;
#ifndef __AROS__
struct DOSBase *__DOSBase;
struct UtilityBase *__UtilityBase;
#endif
#ifdef __AROS__
struct Library *aroscbase;
#endif
struct Library *FileSysBoxBase;

#ifndef ZERO
#define ZERO MKBADDR(NULL)
#endif

//#define DEBUG
#ifdef DEBUG
	#include <debugf.h>
	#define DEBUGF(str,args...) debugf(str, ## args)
#else
	#define DEBUGF(str,args...)
#endif

extern int setup_malloc(void);
extern int cleanup_malloc(void);

extern int ntfs3g_main(struct Message *msg);

#ifdef __AROS__
__startup static AROS_PROCH(startup, argstr, argsize, sysbase) {
	AROS_PROCFUNC_INIT
#else
int startup(void) {
#endif
	int rc = RETURN_FAIL, error = 0;
	struct Process *thisproc;

#ifdef __AROS__
	SysBase = sysbase;
#else
	SysBase = *(struct ExecBase **)4;
#endif

	DEBUGF("ntfs3g_startup: got execbase %#p\n", SysBase);

	thisproc = (struct Process *)FindTask(NULL);

	if (thisproc->pr_CLI != ZERO) {
		DEBUGF("ntfs3g_startup: CLI startup not supported\n");
		return rc;
	}

	struct MsgPort *port = &thisproc->pr_MsgPort;

	DEBUGF("ntfs3g_startup: waiting at port %#p\n", port);

	WaitPort(port);
	struct Message *msg = GetMsg(port);
	if (msg == NULL) goto end;

	if (msg->mn_Node.ln_Name == NULL) {
		DEBUGF("ntfs3g_startup: WB startup not supported\n");
		Forbid();
		ReplyMsg(msg);
		return rc;
	}

	DEBUGF("ntfs3g_startup: got msg %#p\n", msg);

	if (!setup_malloc()) goto end;

	DOSBase = (struct DOSBase *)OpenLibrary((CONST_STRPTR)"dos.library", MIN_OS_VERSION);
	if (DOSBase == NULL) goto end;
#ifndef __AROS__
	__DOSBase = DOSBase;
#endif

	UtilityBase = (struct UtilityBase *)OpenLibrary((CONST_STRPTR)"utility.library", MIN_OS_VERSION);
	if (UtilityBase == NULL) goto end;
#ifndef __AROS__
	__UtilityBase = UtilityBase;
#endif

#ifdef __AROS__
	aroscbase = OpenLibrary((CONST_STRPTR)"arosc.library", MIN_AROSC_VERSION);
	if (aroscbase == NULL) goto end;
#endif

	FileSysBoxBase = OpenLibrary((CONST_STRPTR)"filesysbox.library", MIN_FBX_VERSION);
	if (FileSysBoxBase == NULL) goto end;

	error = ntfs3g_main(msg);
	msg = NULL;
	if (!error) rc = RETURN_OK;

end:
	DEBUGF("ntfs3g_startup: shutting down (err %d, rc %d)\n", error, rc);

	CloseLibrary(FileSysBoxBase);

#ifdef __AROS__
	CloseLibrary(aroscbase);
#endif

	CloseLibrary((struct Library *)UtilityBase);

	CloseLibrary((struct Library *)DOSBase);

	cleanup_malloc();

	if (msg != NULL) {
		struct DosPacket *pkt = (struct DosPacket *)msg->mn_Node.ln_Name;
		struct MsgPort *replyport = pkt->dp_Port;

		pkt->dp_Res1 = DOSFALSE;
		pkt->dp_Res2 = 0;

		PutMsg(replyport, msg);
	}

	DEBUGF("ntfs3g_startup: bye.\n");

	return rc;

#ifdef __AROS__
	AROS_PROCFUNC_EXIT
#endif
}


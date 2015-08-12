#ifndef DISKIO_INTERNAL_H
#define DISKIO_INTERNAL_H

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

#include "diskio.h"
#include <exec/errors.h>
#include <devices/trackdisk.h>
#include <devices/newstyle.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/utility.h>
#include <proto/filesysbox.h>
#include <string.h>

//#define DEBUG
//#define DISABLE_BLOCK_CACHE
#define DISABLE_DOSTYPE_CHECK

/* debugf.c */
int debugf(const char *fmt, ...);
int vdebugf(const char *fmt, va_list args);

#ifdef DEBUG
#define DEBUGF(...) debugf(__VA_ARGS__)
#else
#define DEBUGF(...)
#endif

#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAX(a,b) ((a)>(b)?(a):(b))

#ifndef DISABLE_BLOCK_CACHE
struct BlockCache {
	struct MinNode         node;
	struct DiskIO         *dio_handle;
	APTR                   mempool;
	ULONG                  sector_size;
	struct SignalSemaphore cache_semaphore;
	struct MinList         clean_list;
	struct MinList         dirty_list;
	struct Hook           *cache_tree_hook;
	struct SplayTree      *cache_tree;
	ULONG                  num_clean_nodes;
	ULONG                  num_dirty_nodes;
	ULONG                  num_cache_nodes;
	APTR                   rw_buffer;
	struct Interrupt       mem_handler;
};

struct BlockCacheNode {
	struct MinNode node;
	UQUAD          sector;
	ULONG          checksum;
	LONG           dirty;
	APTR           data;
};
#endif

struct DiskIO {
	APTR               mempool;
	struct MsgPort    *diskmp;
	struct IOExtTD    *diskiotd;
	struct Device     *disk_device;
	UWORD              cmd_support;
	BOOL               use_full_disk;
	ULONG              sector_size;
	ULONG              sector_shift;
	ULONG              sector_mask;
	UQUAD              partition_start;
	UQUAD              partition_size;
	UQUAD              total_sectors;
	BOOL               disk_present;
	BOOL               disk_ok;
	BOOL               write_protected;
	ULONG              disk_id;
	UQUAD              disk_size;
	UWORD              read_cmd;
	UWORD              write_cmd;
	UWORD              update_cmd;
#ifndef DISABLE_BLOCK_CACHE
	BOOL               no_cache;
	BOOL               no_write_cache;
	struct BlockCache *block_cache;
#endif
	APTR               rw_buffer;
	TEXT               devname[256];
	BOOL               inhibit;
	BOOL               uninhibit;
	BOOL               doupdate;
	BOOL               read_only;
};

#define CMDSF_TD32       (1 << 0)
#define CMDSF_ETD32      (1 << 1)
#define CMDSF_TD64       (1 << 2)
#define CMDSF_NSD_TD64   (1 << 3)
#define CMDSF_NSD_ETD64  (1 << 4)
#define CMDSF_CMD_UPDATE (1 << 5)
#define CMDSF_ETD_UPDATE (1 << 6)

#ifndef DISABLE_BLOCK_CACHE
#define MAX_CACHE_NODES 4096
#define MAX_DIRTY_NODES 1024
#define RW_BUFFER_SIZE 128
#define MAX_READ_AHEAD 128
//#define MAX_CACHED_READ 128
//#define MAX_CACHED_WRITE 128
#endif

#ifndef DISABLE_BLOCK_CACHE
/* blockcache.c */
struct BlockCache *InitBlockCache(struct DiskIO *dio);
void CleanupBlockCache(struct BlockCache *bc);
BOOL BlockCacheRetrieve(struct BlockCache *bc, UQUAD sector, APTR buffer, BOOL dirty_only);
BOOL BlockCacheStore(struct BlockCache *bc, UQUAD sector, CONST_APTR buffer, BOOL update_only);
BOOL BlockCacheWrite(struct BlockCache *bc, UQUAD sector, CONST_APTR buffer);
BOOL BlockCacheFlush(struct BlockCache *bc);

/* mergesort.c */
void SortBlockCacheNodes(struct MinList *list);
#endif

/* diskio.c */
void ClearSectorSize(struct DiskIO *dio);
void SetSectorSize(struct DiskIO *dio, ULONG sector_size);
LONG ReadBlocksUncached(struct DiskIO *dio, UQUAD block, APTR buffer, ULONG blocks);
LONG WriteBlocksUncached(struct DiskIO *dio, UQUAD block, CONST_APTR buffer, ULONG blocks);
#ifdef DISABLE_BLOCK_CACHE
#define ReadBlocksCached  ReadBlocksUncached
#define WriteBlocksCached WriteBlocksUncached
#else
LONG ReadBlocksCached(struct DiskIO *dio, UQUAD block, APTR buffer, ULONG blocks);
LONG WriteBlocksCached(struct DiskIO *dio, UQUAD block, CONST_APTR buffer, ULONG blocks);
#endif

#endif

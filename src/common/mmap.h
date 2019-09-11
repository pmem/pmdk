/*
 * Copyright 2014-2019, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *
 *     * Neither the name of the copyright holder nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * mmap.h -- internal definitions for mmap module
 */

#ifndef PMDK_MMAP_H
#define PMDK_MMAP_H 1

#include <stddef.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>

#include "out.h"
#include "queue.h"
#include "os.h"

#ifdef __cplusplus
extern "C" {
#endif

extern int Mmap_no_random;
extern void *Mmap_hint;
extern char *Mmap_mapfile;

void *util_map_sync(void *addr, size_t len, int proto, int flags, int fd,
	os_off_t offset, int *map_sync);
void *util_map(int fd, os_off_t off, size_t len, int flags, int rdonly,
		size_t req_align, int *map_sync);
int util_unmap(void *addr, size_t len);

#ifdef __FreeBSD__
#define MAP_NORESERVE 0
#define OS_MAPFILE "/proc/curproc/map"
#else
#define OS_MAPFILE "/proc/self/maps"
#endif

#ifndef MAP_SYNC
#define MAP_SYNC 0x80000
#endif

#ifndef MAP_SHARED_VALIDATE
#define MAP_SHARED_VALIDATE 0x03
#endif

/*
 * macros for micromanaging range protections for the debug version
 */
#ifdef DEBUG

#define RANGE(addr, len, is_dev_dax, type) do {\
	if (!is_dev_dax) ASSERT(util_range_##type(addr, len) >= 0);\
} while (0)

#else

#define RANGE(addr, len, is_dev_dax, type) do {} while (0)

#endif

#define RANGE_RO(addr, len, is_dev_dax) RANGE(addr, len, is_dev_dax, ro)
#define RANGE_RW(addr, len, is_dev_dax) RANGE(addr, len, is_dev_dax, rw)
#define RANGE_NONE(addr, len, is_dev_dax) RANGE(addr, len, is_dev_dax, none)

/* pmem mapping type */
enum pmem_map_type {
	PMEM_DEV_DAX,	/* device dax */
	PMEM_MAP_SYNC,	/* mapping with MAP_SYNC flag on dax fs */

	MAX_PMEM_TYPE
};

/*
 * this structure tracks the file mappings outstanding per file handle
 */
struct map_tracker {
	PMDK_SORTEDQ_ENTRY(map_tracker) entry;
	uintptr_t base_addr;
	uintptr_t end_addr;
	int region_id;
	enum pmem_map_type type;
#ifdef _WIN32
	/* Windows-specific data */
	HANDLE FileHandle;
	HANDLE FileMappingHandle;
	DWORD Access;
	os_off_t Offset;
	size_t FileLen;
#endif
};

void util_mmap_init(void);
void util_mmap_fini(void);

int util_range_ro(void *addr, size_t len);
int util_range_rw(void *addr, size_t len);
int util_range_none(void *addr, size_t len);

char *util_map_hint_unused(void *minaddr, size_t len, size_t align);
char *util_map_hint(size_t len, size_t req_align);

#define MEGABYTE ((uintptr_t)1 << 20)
#define GIGABYTE ((uintptr_t)1 << 30)

/*
 * util_map_hint_align -- choose the desired mapping alignment
 *
 * The smallest supported alignment is 2 megabytes because of the object
 * alignment requirements. Changing this value to 4 kilobytes constitues a
 * layout change.
 *
 * Use 1GB page alignment only if the mapping length is at least
 * twice as big as the page size.
 */
static inline size_t
util_map_hint_align(size_t len, size_t req_align)
{
	size_t align = 2 * MEGABYTE;
	if (req_align)
		align = req_align;
	else if (len >= 2 * GIGABYTE)
		align = GIGABYTE;

	return align;
}

int util_range_register(const void *addr, size_t len, const char *path,
		enum pmem_map_type type);
int util_range_unregister(const void *addr, size_t len);
struct map_tracker *util_range_find(uintptr_t addr, size_t len);
int util_range_is_pmem(const void *addr, size_t len);

#ifdef __cplusplus
}
#endif
#endif

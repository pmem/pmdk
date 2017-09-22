/*
 * Copyright 2017, Intel Corporation
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
 * badblock_filesrc_linux.c - implementation of linux badblock file source
 *
 * Badblocks for a particular file can only be found by looking at the
 * underlying device and then filtering out the badblocks that do not overlap
 * with the extents on which the file is located.
 *
 * To achieve the above, we are using only the kernel exposed features:
 * the region badblocks list and file-system extents list.
 *
 * To clear a badblock, we are using a the kernel provided feature that
 * automatically discards the poisoned page when we hole-punch it at the
 * appropriate offset in the file.
 */
#define _GNU_SOURCE

#include <fcntl.h>
#include <inttypes.h>
#include "file.h"
#include "os.h"
#include "out.h"
#include "sysfs.h"
#include "extent.h"
#include "plugin.h"
#include "badblock_filesrc.h"

struct badblock {
	const char *file;
	uint64_t offset_physical;
	uint64_t offset_logical;
	uint64_t length;
};

struct badblock_iter_file {
	struct {
		int (*next)(struct badblock_iter_file *iter,
			struct badblock *b);
		int (*clear)(struct badblock_iter_file *iter,
			struct badblock *b);
		void (*del)(struct badblock_iter_file *iter);
	} i_ops;

	struct sysfs_iter *badblock_iter;
	size_t sector_size;
	int fd;
	char *file;
	size_t nextents;
	struct extent extents[];
};

/*
 * badblock_find_extent -- finds the badblock in the file extents
 */
static int
badblock_find_extent(struct badblock_iter_file *iter, struct badblock *b)
{
	for (int i = 0; i < (int)iter->nextents; ++i) {
		struct extent *cur = &iter->extents[i];

		if (cur->offset_physical <= b->offset_physical + b->length &&
			cur->offset_physical + cur->length > b->offset_physical)
			return i;
	}

	return -1;
}

/*
 * badblock_next -- returns the current badblock and bumps the iterator position
 */
static int
badblock_next(struct badblock_iter_file *iter, struct badblock *badblock)
{
	LOG(3, "iter %p badblock %p", iter, badblock);

	int extent_id;
	do {
		if (sysfs_next(iter->badblock_iter,
			&badblock->offset_physical, &badblock->length) != 2)
				return -1;
	} while ((extent_id = badblock_find_extent(iter, badblock)) == -1);

	struct extent *e = &iter->extents[extent_id];

	badblock->file = iter->file;

	badblock->length *= iter->sector_size;
	badblock->offset_physical *= iter->sector_size;
	int64_t off = (int64_t)e->offset_physical -
		(int64_t)badblock->offset_physical;
	if (off < 0)
		off = 0;

	badblock->offset_logical = e->offset_logical + (uint64_t)e;

	return 0;
}

/*
 * badblock_del -- deletes the badblock iterator instance
 */
static void
badblock_del(struct badblock_iter_file *iter)
{
	close(iter->fd);
	sysfs_delete(iter->badblock_iter);
	Free(iter->file);
	Free(iter);
}

/*
 * badblock_clear -- clear a badblock
 */
static int
badblock_clear(struct badblock_iter_file *iter, struct badblock *b)
{
	LOG(3, "length %" PRIu64
		" offset logical %" PRIu64
		" offset physical %" PRIu64,
		b->length, b->offset_logical, b->offset_physical);

	ASSERTne(iter->nextents, 0);
	int extent_id = badblock_find_extent(iter, b);
	ASSERTne(extent_id, -1);

	struct extent *extent = &iter->extents[extent_id];

	ASSERT(extent->offset_logical >= b->offset_logical);

	uint64_t file_offset = extent->offset_logical - b->offset_logical;

	return fallocate(iter->fd, FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE,
		(long)file_offset, (long)b->length);
}

/*
 * iter_from_file -- creates a new badblock iterator
 */
static struct badblock_iter_file *
iter_from_file(const char *file)
{
	LOG(3, "%s", file);

	int fd = os_open(file, O_RDONLY);
	if (fd < 0)
		goto error_file_open;

	if (util_fd_is_device_dax(fd))
		goto error_devdax;

	struct extent_iter *eiter = extent_new(fd);
	if (eiter == NULL)
		goto error_extent_iter;

	struct badblock_iter_file *iter = Malloc(sizeof(*iter) +
		sizeof(struct extent) * extent_count(eiter));

	if (iter == NULL)
		goto error_iter_alloc;

	iter->nextents = 0;

	while (extent_next(eiter, &iter->extents[iter->nextents++]) == 0)
		;

	if (sysfs_dev_single(fd,
		"queue/hw_sector_size", "%lu", &iter->sector_size) != 1)
		goto error_sector_size_read;

	iter->fd = fd;

	unsigned region_id;
	unsigned namespace_id;

	if (sysfs_dev_single(fd, "device/namespace", "namespace%u.%u",
		&region_id, &namespace_id) != 2)
		goto error_ns_query;

	char badblocks_subpath[PATH_MAX];
	if (sprintf(badblocks_subpath,
		"device/subsystem/devices/region%u/badblocks",
		region_id) < 0)
		goto error_ns_query;

	iter->badblock_iter = sysfs_dev_new(fd, badblocks_subpath, "%lu %lu");
	if (iter->badblock_iter == NULL)
		goto error_sysfs_alloc;

	iter->file = Strdup(file);
	if (iter->file == NULL)
		goto error_file_alloc;

	extent_delete(eiter);

	iter->i_ops.next = badblock_next;
	iter->i_ops.clear = badblock_clear;
	iter->i_ops.del = badblock_del;

	return iter;

error_file_alloc:
	sysfs_delete(iter->badblock_iter);
error_sysfs_alloc:
error_ns_query:
error_sector_size_read:
	Free(iter);
error_iter_alloc:
	extent_delete(eiter);
error_devdax:
error_extent_iter:
	close(fd);
error_file_open:
	return NULL;
}

/*
 * pmem_plugin_desc -- linux file badblock descriptor
 */
static void
pmem_plugin_desc(const char **module_name,
	const char **name, unsigned *version, void **funcs)
{
	*module_name = "badblock_source";
	*name = "builtin_file_source";
	*version = 1;
	*funcs = iter_from_file;
}

/*
 * pmem_plugin_load -- noop
 */
static int
pmem_plugin_load(void)
{
	return 0;
}

/*
 * pmem_plugin_unload -- noop
 */
static void
pmem_plugin_unload(void)
{

}

/*
 * Linux file badblock plugin
 */
static struct plugin_ops badblock_file_source_plugin = {
	pmem_plugin_desc, pmem_plugin_load, pmem_plugin_unload,
};

/*
 * badblock_file_source_add -- registers the file badblock source
 */
void
badblock_file_source_add(void)
{
	plugin_add(&badblock_file_source_plugin);
}

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
 * badblocks_devdax.c - implementation of linux badblock devdax source
 *
 * This plugin leverages the ndctl implementation of the ACPI 6.2 nvdimm related
 * features: Address Range Scrub (ARS) and the Clear Uncorrectable Error
 * function.
 *
 * There's no need to be selective of badblocks, because the pool can be created
 * only on the whole device - and that means that a badblock anywhere on the
 * device dax can potentially corrupt the pool.
 */
#define _GNU_SOURCE

#include <fcntl.h>
#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

#include <ndctl/libndctl.h>
#include <linux/ndctl.h>

#define DEFAULT_SECTOR_SIZE 512

void pmem_plugin_desc(const char **module_name,
	const char **name, unsigned *version, void **funcs);
int pmem_plugin_load(void);
void pmem_plugin_unload(void);

struct badblock_pmem {
	const char *file;
	uint64_t offset;
	uint64_t length;
};

struct badblock_iter_dax {
	struct {
		int (*next)(struct badblock_iter_dax *iter,
			struct badblock_pmem *b);
		int (*clear)(struct badblock_iter_dax *iter,
			struct badblock_pmem *b);
		void (*del)(struct badblock_iter_dax *iter);
	} i_ops;
	char *file;

	struct ndctl_ctx *ctx;
	struct ndctl_dax *dax;

	size_t pos;
	size_t sector_size;

	size_t nbadblocks;
	struct badblock_pmem *badblocks;
};

/*
 * badblock_next -- returns the current badblock and bumps the iterator position
 */
static int
badblock_next(struct badblock_iter_dax *iter, struct badblock_pmem *badblock)
{
	if (iter->pos >= iter->nbadblocks)
		return -1;

	*badblock = iter->badblocks[iter->pos++];

	return 0;
}

/*
 * badblock_del -- deletes the badblock iterator instance
 */
static void
badblock_del(struct badblock_iter_dax *iter)
{
	ndctl_unref(iter->ctx);
	free(iter->badblocks);
	free(iter->file);
	free(iter);
}

/*
 * badblock_clear -- clear a badblock
 */
static int
badblock_clear(struct badblock_iter_dax *iter, struct badblock_pmem *b)
{
	size_t addr = b->offset;
	size_t len = b->length;

	size_t base = ndctl_dax_get_resource(iter->dax);
	addr += base;

	int ret = 0;

	struct ndctl_cmd *cmd_ars_cap = ndctl_bus_cmd_new_ars_cap(
		ndctl_dax_get_bus(iter->dax), addr, len);

	if ((ret = ndctl_cmd_submit(cmd_ars_cap)) < 0)
		goto out_ars_cap;

	struct ndctl_cmd *cmd_ars_start =
		ndctl_bus_cmd_new_ars_start(cmd_ars_cap, ND_ARS_PERSISTENT);

	if ((ret = ndctl_cmd_submit(cmd_ars_start)) < 0)
		goto out_ars_start;

	struct ndctl_cmd *cmd_ars_status;
	do {
		cmd_ars_status = ndctl_bus_cmd_new_ars_status(cmd_ars_cap);
		if ((ret = ndctl_cmd_submit(cmd_ars_status)) < 0)
			goto out_ars_status;
	} while (ndctl_cmd_ars_in_progress(cmd_ars_status));

	struct ndctl_range range;
	ndctl_cmd_ars_cap_get_range(cmd_ars_cap, &range);

	struct ndctl_cmd *cmd_clear_error = ndctl_bus_cmd_new_clear_error(
		range.address, range.length, cmd_ars_cap);

	if ((ret = ndctl_cmd_submit(cmd_clear_error)) < 0)
		goto out_clear_error;

	size_t cleared = ndctl_cmd_clear_error_get_cleared(cmd_clear_error);
	ret = cleared == len ? 0 : -1;

out_clear_error:
	ndctl_cmd_unref(cmd_clear_error);
out_ars_status:
	ndctl_cmd_unref(cmd_ars_status);
out_ars_start:
	ndctl_cmd_unref(cmd_ars_start);
out_ars_cap:
	ndctl_cmd_unref(cmd_ars_cap);

	return ret;
}

/*
 * badblock_find_dax -- finds the corresponding device dax in the hierarchy
 */
static struct ndctl_dax *
badblock_find_dax(struct badblock_iter_dax *iter, const char *path)
{
	const char *devname = strrchr(path, '/');
	if (devname == NULL)
		return NULL;

	devname += 1; /* skip the '/' */

	struct ndctl_bus *bus;
	struct ndctl_region *region;
	struct ndctl_dax *dax;
	ndctl_bus_foreach(iter->ctx, bus) {
		ndctl_region_foreach(bus, region) {
			ndctl_dax_foreach(region, dax) {
				if (strcmp(devname,
					ndctl_dax_get_devname(dax)) == 0)
					return dax;
			}
		}
	}

	return NULL;
}

#define BADBLOCK_ARR_INCREMENT 10

/*
 * iter_from_file -- creates a new badblock iterator
 */
static struct badblock_iter_dax *
iter_from_file(const char *file)
{
	struct badblock_iter_dax *iter = malloc(sizeof(*iter));
	if (iter == NULL)
		return NULL;

	if (ndctl_new(&iter->ctx) != 0)
		goto error_ndctl_new;

	if ((iter->dax = badblock_find_dax(iter, file)) == NULL)
		goto error_no_dax;

	iter->file = strdup(file);
	if (iter->file == NULL)
		goto error_file_alloc;

	iter->i_ops.next = badblock_next;
	iter->i_ops.clear = badblock_clear;
	iter->i_ops.del = badblock_del;

	/* reading the sector size is very unreliable, so assume the default */
	iter->sector_size = DEFAULT_SECTOR_SIZE;

	iter->nbadblocks = 0;
	iter->badblocks = NULL;
	size_t allocated_badblocks = 0;
	struct badblock *b;
	struct badblock_pmem *rb;

	struct ndctl_region *region = ndctl_dax_get_region(iter->dax);
	intptr_t base_pos = (intptr_t)ndctl_region_get_resource(region);
	intptr_t dax_pos = (intptr_t)ndctl_dax_get_resource(iter->dax);
	intptr_t dax_len = (intptr_t)ndctl_dax_get_size(iter->dax);
	assert(dax_pos >= base_pos);
	intptr_t dax_offset = dax_pos - base_pos;

	intptr_t b_offset;
	intptr_t b_end;

	ndctl_region_badblock_foreach(region, b) {
		b_offset = (intptr_t)(b->offset * iter->sector_size) -
			dax_offset;
		b_end = b_offset + (intptr_t)(b->len * iter->sector_size);

		if (b_end < 0 || b_offset > dax_len)
			continue;

		struct badblock_pmem bp;
		bp.offset = b_offset < 0 ? 0 : (uint64_t)b_offset;
		if (b_end > dax_len)
			b_end = dax_len;

		bp.length = (uint64_t)b_end - bp.offset;
		bp.file = iter->file;

		if (iter->nbadblocks >= allocated_badblocks) {
			rb = realloc(iter->badblocks,
				sizeof(struct badblock_pmem) *
				(allocated_badblocks + BADBLOCK_ARR_INCREMENT));
			if (rb == NULL)
				goto error_badblock_iter;
			iter->badblocks = rb;
			allocated_badblocks += BADBLOCK_ARR_INCREMENT;
		}

		iter->badblocks[iter->nbadblocks++] = bp;
	}

	iter->pos = 0;

	return iter;

error_badblock_iter:
	free(iter->badblocks);
	free(iter->file);
error_file_alloc:
error_no_dax:
	ndctl_unref(iter->ctx);
error_ndctl_new:
	free(iter);

	return NULL;
}

/*
 * pmem_plugin_desc -- linux file badblock descriptor
 */
void
pmem_plugin_desc(const char **module_name,
	const char **name, unsigned *version, void **funcs)
{
	*module_name = "badblock_source";
	*name = "devdax_badblock_source";
	*version = 1;
	*funcs = iter_from_file;
}

/*
 * pmem_plugin_load -- noop
 */
int
pmem_plugin_load(void)
{
	return 0;
}

/*
 * pmem_plugin_unload -- noop
 */
void
pmem_plugin_unload(void)
{

}

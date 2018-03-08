/*
 * Copyright 2017-2018, Intel Corporation
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
 * os_dimm_ndctl.c -- implementation of DIMMs API based on the ndctl library
 */
#define _GNU_SOURCE

#include <sys/types.h>
#include <libgen.h>
#include <limits.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ndctl/libndctl.h>
#include <ndctl/libdaxctl.h>
#include <linux/ndctl.h>

#include "out.h"
#include "os.h"
#include "os_dimm.h"
#include "os_badblock.h"

/* XXX: workaround for missing PAGE_SIZE - should be fixed in linux/ndctl.h */
#include <sys/user.h>
#include <linux/ndctl.h>

#include "out.h"
#include "os.h"
#include "os_dimm.h"
#include "os_badblock.h"

/*
 * http://pmem.io/documents/NVDIMM_DSM_Interface-V1.6.pdf
 * Table 3-2 SMART amd Health Data - Validity flags
 * Bit[5] – If set to 1, indicates that Unsafe Shutdown Count
 * field is valid
 */
#define USC_VALID_FLAG (1 << 5)

#define FOREACH_BUS_REGION_NAMESPACE(ctx, bus, region, ndns)	\
	ndctl_bus_foreach(ctx, bus)				\
		ndctl_region_foreach(bus, region)		\
			ndctl_namespace_foreach(region, ndns)	\

/*
 * os_dimm_region_namespace -- (internal) returns the region
 *                             (and optionally the namespace)
 *                             where the given file is located
 */
static int
os_dimm_region_namespace(struct ndctl_ctx *ctx, const os_stat_t *st,
				struct ndctl_region **pregion,
				struct ndctl_namespace **pndns)
{
	LOG(3, "ctx %p stat %p pregion %p pnamespace %p",
		ctx, st, pregion, pndns);

	struct ndctl_bus *bus;
	struct ndctl_region *region;
	struct ndctl_namespace *ndns;
	dev_t dev = S_ISCHR(st->st_mode) ? st->st_rdev : st->st_dev;

	ASSERTne(pregion, NULL);
	*pregion = NULL;

	if (pndns)
		*pndns = NULL;

	FOREACH_BUS_REGION_NAMESPACE(ctx, bus, region, ndns) {
		struct ndctl_btt *btt;
		struct ndctl_dax *dax = NULL;
		struct ndctl_pfn *pfn;
		const char *devname;

		if ((btt = ndctl_namespace_get_btt(ndns))) {
			devname = ndctl_btt_get_block_device(btt);
		} else if ((pfn = ndctl_namespace_get_pfn(ndns))) {
			devname = ndctl_pfn_get_block_device(pfn);
		} else if ((dax = ndctl_namespace_get_dax(ndns))) {
			struct daxctl_region *dax_region;
			dax_region = ndctl_dax_get_daxctl_region(dax);
			/* there is always one dax device in dax_region */
			if (dax_region) {
				struct daxctl_dev *dev;
				dev = daxctl_dev_get_first(dax_region);
				devname = daxctl_dev_get_devname(dev);
			} else {
				ERR("cannot find dax region");
				return -1;
			}
		} else {
			devname = ndctl_namespace_get_block_device(ndns);
		}

		if (*devname == '\0')
			continue;

		char path[PATH_MAX];
		os_stat_t stat;
		if (snprintf(path, PATH_MAX, "/dev/%s", devname) == -1) {
			ERR("!snprintf");
			return -1;
		}

		if (os_stat(path, &stat)) {
			ERR("!stat %s", path);
			return -1;
		}

		if (dev == stat.st_rdev) {
			LOG(4, "found matching device: %s", path);

			*pregion = region;

			if (pndns)
				*pndns = ndns;

			return 0;
		}

		LOG(10, "skipping not matching device: %s", path);
	}

	LOG(10, "did not found any matching device");

	return 0;
}

/*
 * os_dimm_interleave_set -- (internal) returns set of dimms
 *                           where the pool file is located
 */
static struct ndctl_interleave_set *
os_dimm_interleave_set(struct ndctl_ctx *ctx, const os_stat_t *st)
{
	LOG(3, "ctx %p stat %p", ctx, st);

	struct ndctl_region *region = NULL;

	if (os_dimm_region_namespace(ctx, st, &region, NULL))
		return NULL;

	return region ? ndctl_region_get_interleave_set(region) : NULL;
}

/*
 * os_dimm_uid -- returns a file uid based on dimms uids
 *
 * if uid == null then function will return required buffer size
 */
int
os_dimm_uid(const char *path, char *uid, size_t *buff_len)
{
	LOG(3, "path %s, uid %p, len %lu", path, uid, *buff_len);

	os_stat_t st;

	struct ndctl_ctx *ctx;
	struct ndctl_interleave_set *set;
	struct ndctl_dimm *dimm;
	int ret = 0;
	if (os_stat(path, &st)) {
		ERR("!stat %s", path);
		return -1;
	}

	if (ndctl_new(&ctx)) {
		ERR("!ndctl_new");
		return -1;
	}

	if (uid == NULL) {
		*buff_len = 1; /* '\0' */
	}

	set = os_dimm_interleave_set(ctx, &st);
	if (set == NULL)
		goto end;

	if (uid == NULL) {
		ndctl_dimm_foreach_in_interleave_set(set, dimm) {
			*buff_len += strlen(ndctl_dimm_get_unique_id(dimm));
		}
		goto end;
	}
	size_t len = 1;
	ndctl_dimm_foreach_in_interleave_set(set, dimm) {
		const char *dimm_uid = ndctl_dimm_get_unique_id(dimm);
		len += strlen(dimm_uid);
		if (len > *buff_len) {
			ret = -1;
			goto end;
		}
		strncat(uid, dimm_uid, *buff_len);
	}
end:
	ndctl_unref(ctx);
	return ret;
}

/*
 * os_dimm_usc -- returns unsafe shutdown count
 */
int
os_dimm_usc(const char *path, uint64_t *usc)
{
	LOG(3, "path %s, uid %p", path, usc);

	os_stat_t st;
	struct ndctl_ctx *ctx;

	*usc = 0;

	if (os_stat(path, &st)) {
		ERR("!stat %s", path);
		return -1;
	}

	if (ndctl_new(&ctx)) {
		ERR("!ndctl_new");
		return -1;
	}

	struct ndctl_interleave_set *iset =
		os_dimm_interleave_set(ctx, &st);

	if (iset == NULL)
		goto out;

	struct ndctl_dimm *dimm;

	ndctl_dimm_foreach_in_interleave_set(iset, dimm) {
		struct ndctl_cmd *cmd = ndctl_dimm_cmd_new_smart(dimm);

		if (ndctl_cmd_submit(cmd))
			goto out;

		if (!(ndctl_cmd_smart_get_flags(cmd) & USC_VALID_FLAG))
			goto out;

		*usc += ndctl_cmd_smart_get_shutdown_count(cmd);
	}
out:
	ndctl_unref(ctx);
	return 0;
}

/*
 * os_dimm_get_namespace_bounds -- (internal) returns the bounds
 *                                 (offset and size) of the given namespace
 *                                 relative to the beginning of its region
 */
static void
os_dimm_get_namespace_bounds(struct ndctl_region *region,
				struct ndctl_namespace *ndns,
				unsigned long long *ns_offset,
				unsigned long long *ns_size)
{
	LOG(3, "region %p namespace %p ns_offset %p ns_size %p",
		region, ndns, ns_offset, ns_size);

	struct ndctl_pfn *pfn = ndctl_namespace_get_pfn(ndns);
	struct ndctl_dax *dax = ndctl_namespace_get_dax(ndns);

	ASSERTne(ns_offset, NULL);
	ASSERTne(ns_size, NULL);

	if (pfn) {
		*ns_offset = ndctl_pfn_get_resource(pfn);
		*ns_size = ndctl_pfn_get_size(pfn);
	} else if (dax) {
		*ns_offset = ndctl_dax_get_resource(dax);
		*ns_size = ndctl_dax_get_size(dax);
	} else { /* raw or btt */
		*ns_offset = ndctl_namespace_get_resource(ndns);
		*ns_size = ndctl_namespace_get_size(ndns);
	}

	*ns_offset -= ndctl_region_get_resource(region);
}

/*
 * os_dimm_namespace_get_badblocks -- (internal) returns bad blocks
 *                                    in the given namespace
 */
static int
os_dimm_namespace_get_badblocks(struct ndctl_region *region,
				struct ndctl_namespace *ndns,
				struct badblocks *bbs)
{
	LOG(3, "region %p, namespace %p", region, ndns);

	ASSERTne(bbs, NULL);

	unsigned long long ns_beg, ns_size, ns_end;
	unsigned long long bb_beg, bb_end;
	unsigned long long beg, end;

	struct bad_block *bbvp = NULL;
	struct bad_block *newbbvp;
	unsigned bb_count = 0;

	bbs->ns_resource = 0;
	bbs->bb_cnt = 0;
	bbs->bbv = NULL;

	os_dimm_get_namespace_bounds(region, ndns, &ns_beg, &ns_size);

	ns_end = ns_beg + ns_size - 1;

	LOG(10, "namespace: begin %llu, end %llu size %llu (in 512B sectors)",
		B2SEC(ns_beg), B2SEC(ns_end + 1) - 1, B2SEC(ns_size));

	struct badblock *bb;
	ndctl_region_badblock_foreach(region, bb) {
		bb_beg = SEC2B(bb->offset);
		bb_end = bb_beg + SEC2B(bb->len) - 1;

		LOG(10,
			"region bad block: begin %llu end %llu length %u (in 512B sectors)",
			bb->offset, bb->offset + bb->len - 1, bb->len);

		if (bb_beg > ns_end || ns_beg > bb_end)
			continue;

		beg = (bb_beg > ns_beg) ? bb_beg : ns_beg;
		end = (bb_end < ns_end) ? bb_end : ns_end;

		newbbvp = Realloc(bbvp, (++bb_count) *
					sizeof(struct bad_block));
		if (newbbvp == NULL) {
			ERR("!realloc");
			Free(bbvp);
			return -1;
		}

		bbvp = newbbvp;
		bbvp[bb_count - 1].offset = beg - ns_beg;
		bbvp[bb_count - 1].length = (unsigned)(end - beg + 1);

		LOG(4,
			"namespace bad block: begin %llu end %llu length %llu (in 512B sectors)",
			B2SEC(beg - ns_beg), B2SEC(end - ns_beg),
			B2SEC(end - beg) + 1);
	}

	LOG(4, "number of bad blocks detected: %u", bb_count);

	bbs->bb_cnt = bb_count;
	bbs->bbv = bbvp;
	bbs->ns_resource = ns_beg + ndctl_region_get_resource(region);

	return 0;
}

/*
 * os_dimm_files_namespace_badblocks_bus -- (internal) returns badblocks
 *                                          in the namespace where the given
 *                                          file is located
 *                                          (optionally returns also the bus)
 */
static int
os_dimm_files_namespace_badblocks_bus(struct ndctl_ctx *ctx,
					const char *path,
					struct ndctl_bus **pbus,
					struct badblocks *bbs)
{
	LOG(3, "ctx %p path %s pbus %p", ctx, path, pbus);

	struct ndctl_region *region;
	struct ndctl_namespace *ndns;

	os_stat_t st;

	if (os_stat(path, &st)) {
		ERR("!stat %s", path);
		return -1;
	}

	int rv = os_dimm_region_namespace(ctx, &st, &region, &ndns);
	if (rv) {
		ERR("getting region and namespace failed");
		return -1;
	}

	if (region == NULL || ndns == NULL)
		return 0;

	if (pbus)
		*pbus = ndctl_region_get_bus(region);

	return os_dimm_namespace_get_badblocks(region, ndns, bbs);
}

/*
 * os_dimm_files_namespace_badblocks -- returns badblocks in the namespace
 *                                      where the given file is located
 */
int
os_dimm_files_namespace_badblocks(const char *path, struct badblocks *bbs)
{
	LOG(3, "path %s", path);

	struct ndctl_ctx *ctx;

	if (ndctl_new(&ctx)) {
		ERR("!ndctl_new");
		return -1;
	}

	int ret = os_dimm_files_namespace_badblocks_bus(ctx, path, NULL, bbs);

	ndctl_unref(ctx);

	return ret;
}

/*
 * os_dimm_devdax_clear_one_badblock -- (internal) clear one bad block
 *                                      in the dax device
 */
static int
os_dimm_devdax_clear_one_badblock(struct ndctl_bus *bus,
				unsigned long long address,
				unsigned long long length)
{
	LOG(3, "bus %p address 0x%llx length %llu (bytes)",
		bus, address, length);

	int ret = 0;

	struct ndctl_cmd *cmd_ars_cap = ndctl_bus_cmd_new_ars_cap(bus,
							address, length);
	if (cmd_ars_cap == NULL) {
		ERR("failed to create cmd (bus '%s')",
			ndctl_bus_get_provider(bus));
		return -1;
	}

	if ((ret = ndctl_cmd_submit(cmd_ars_cap)) < 0) {
		ERR("failed to submit cmd (bus '%s')",
			ndctl_bus_get_provider(bus));
		goto out_ars_cap;
	}

	struct ndctl_cmd *cmd_ars_start =
		ndctl_bus_cmd_new_ars_start(cmd_ars_cap, ND_ARS_PERSISTENT);
	if (cmd_ars_start == NULL) {
		ERR("ndctl_bus_cmd_new_ars_start() failed");
		goto out_ars_cap;
	}

	if ((ret = ndctl_cmd_submit(cmd_ars_start)) < 0) {
		ERR("failed to submit cmd (bus '%s')",
			ndctl_bus_get_provider(bus));
		goto out_ars_start;
	}

	struct ndctl_cmd *cmd_ars_status;
	do {
		cmd_ars_status = ndctl_bus_cmd_new_ars_status(cmd_ars_cap);
		if (cmd_ars_status == NULL) {
			ERR("ndctl_bus_cmd_new_ars_status() failed");
			goto out_ars_start;
		}

		if ((ret = ndctl_cmd_submit(cmd_ars_status)) < 0) {
			ERR("failed to submit cmd (bus '%s')",
				ndctl_bus_get_provider(bus));
			goto out_ars_status;
		}

	} while (ndctl_cmd_ars_in_progress(cmd_ars_status));

	struct ndctl_range range;
	ndctl_cmd_ars_cap_get_range(cmd_ars_cap, &range);

	struct ndctl_cmd *cmd_clear_error = ndctl_bus_cmd_new_clear_error(
		range.address, range.length, cmd_ars_cap);

	if ((ret = ndctl_cmd_submit(cmd_clear_error)) < 0) {
		ERR("failed to submit cmd (bus '%s')",
			ndctl_bus_get_provider(bus));
		goto out_clear_error;
	}

	size_t cleared = ndctl_cmd_clear_error_get_cleared(cmd_clear_error);

	LOG(4, "cleared %zu out of %llu bad blocks", cleared, length);

	ret = cleared == length ? 0 : -1;

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
 * os_dimm_devdax_clear_badblocks -- clear all bad blocks in the dax device
 */
int
os_dimm_devdax_clear_badblocks(const char *path)
{
	LOG(3, "path %s", path);

	struct ndctl_ctx *ctx;
	struct ndctl_bus *bus;
	struct badblocks *bbs;
	int ret;

	if (ndctl_new(&ctx)) {
		ERR("!ndctl_new");
		return -1;
	}

	bbs = Zalloc(sizeof(struct badblocks));
	if (bbs == NULL) {
		ERR("!malloc");
		return -1;
	}

	ret = os_dimm_files_namespace_badblocks_bus(ctx, path, &bus, bbs);
	if (ret) {
		ERR("getting bad blocks for the file failed -- %s", path);
		goto exit_free_all;
	}

	if (bbs->bb_cnt == 0 || bbs->bbv == NULL) /* OK - no bad blocks found */
		goto exit_free_all;

	LOG(4, "clearing %u bad block(s)...", bbs->bb_cnt);

	unsigned b;
	for (b = 0; b < bbs->bb_cnt; b++) {
		LOG(4,
			"clearing bad block: offset %llu length %u (in 512B sectors)",
			B2SEC(bbs->bbv[b].offset), B2SEC(bbs->bbv[b].length));

		ret = os_dimm_devdax_clear_one_badblock(bus,
					bbs->bbv[b].offset + bbs->ns_resource,
					bbs->bbv[b].length);
		if (ret) {
			ERR(
				"failed to clear bad block: offset %llu length %u (in 512B sectors)",
				B2SEC(bbs->bbv[b].offset),
				B2SEC(bbs->bbv[b].length));
		}
	}

exit_free_all:
	if (bbs) {
		Free(bbs->bbv);
		Free(bbs);
	}

	ndctl_unref(ctx);

	return ret;
}

// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2017-2020, Intel Corporation */

/*
 * badblocks_ndctl.c -- implementation of DIMMs API based on the ndctl library
 */
#define _GNU_SOURCE

#include <sys/types.h>
#include <libgen.h>
#include <limits.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/sysmacros.h>
#include <fcntl.h>
#include <ndctl/libndctl.h>
#include <ndctl/libdaxctl.h>

#include "file.h"
#include "out.h"
#include "os.h"
#include "badblocks.h"
#include "os_badblock.h"
#include "badblock.h"
#include "vec.h"

#include "libpmem2.h"
#include "pmem2_utils.h"

#define FOREACH_BUS_REGION_NAMESPACE(ctx, bus, region, ndns)	\
	ndctl_bus_foreach(ctx, bus)				\
		ndctl_region_foreach(bus, region)		\
			ndctl_namespace_foreach(region, ndns)	\

/*
 * badblocks_match_devdax -- (internal) returns 1 if the devdax matches
 *                         with the given file, 0 if it doesn't match,
 *                         and -1 in case of error.
 */
static int
badblocks_match_devdax(const os_stat_t *st, const char *devname)
{
	LOG(3, "st %p devname %s", st, devname);

	if (*devname == '\0')
		return 0;

	char path[PATH_MAX];
	os_stat_t stat;
	if (util_snprintf(path, PATH_MAX, "/dev/%s", devname) < 0) {
		ERR("!snprintf");
		return -1;
	}

	if (os_stat(path, &stat)) {
		ERR("!stat %s", path);
		return -1;
	}

	if (st->st_rdev == stat.st_rdev) {
		LOG(4, "found matching device: %s", path);
		return 1;
	}

	LOG(10, "skipping not matching device: %s", path);
	return 0;
}

#define BUFF_LENGTH 64

/*
 * badblocks_match_fsdax -- (internal) returns 1 if the device matches
 *                         with the given file, 0 if it doesn't match,
 *                         and -1 in case of error.
 */
static int
badblocks_match_fsdax(const os_stat_t *st, const char *devname)
{
	LOG(3, "st %p devname %s", st, devname);

	if (*devname == '\0')
		return 0;

	char path[PATH_MAX];
	char dev_id[BUFF_LENGTH];

	if (util_snprintf(path, PATH_MAX, "/sys/block/%s/dev", devname) < 0) {
		ERR("!snprintf");
		return -1;
	}

	if (util_snprintf(dev_id, BUFF_LENGTH, "%d:%d",
			major(st->st_dev), minor(st->st_dev)) < 0) {
		ERR("!snprintf");
		return -1;
	}

	int fd = os_open(path, O_RDONLY);
	if (fd < 0) {
		ERR("!open \"%s\"", path);
		return -1;
	}

	char buff[BUFF_LENGTH];
	ssize_t nread = read(fd, buff, BUFF_LENGTH);
	if (nread < 0) {
		ERR("!read");
		os_close(fd);
		return -1;
	}

	os_close(fd);

	if (nread == 0) {
		ERR("%s is empty", path);
		return -1;
	}

	if (buff[nread - 1] != '\n') {
		ERR("%s doesn't end with new line", path);
		return -1;
	}

	buff[nread - 1] = '\0';

	if (strcmp(buff, dev_id) == 0) {
		LOG(4, "found matching device: %s", path);
		return 1;
	}

	LOG(10, "skipping not matching device: %s", path);
	return 0;
}

/*
 * badblocks_region_namespace -- (internal) returns the region
 *                             (and optionally the namespace)
 *                             where the given file is located
 */
static int
badblocks_region_namespace(struct ndctl_ctx *ctx, const os_stat_t *st,
				struct ndctl_region **pregion,
				struct ndctl_namespace **pndns)
{
	LOG(3, "ctx %p stat %p pregion %p pnamespace %p",
		ctx, st, pregion, pndns);

	struct ndctl_bus *bus;
	struct ndctl_region *region;
	struct ndctl_namespace *ndns;

	ASSERTne(pregion, NULL);
	*pregion = NULL;

	if (pndns)
		*pndns = NULL;

	enum pmem2_file_type pmem2_type;

	int ret = pmem2_get_type_from_stat(st, &pmem2_type);
	if (ret) {
		errno = pmem2_err_to_errno(ret);
		return -1;
	}

	switch (pmem2_type) {
		case PMEM2_FTYPE_REG:
		case PMEM2_FTYPE_DIR:
		case PMEM2_FTYPE_DEVDAX:
			break;
		default:
			ASSERTinfo(0,
				"unhandled file type in pmem2_get_type_from_stat");
			return -1;
	};

	FOREACH_BUS_REGION_NAMESPACE(ctx, bus, region, ndns) {
		struct ndctl_btt *btt;
		struct ndctl_dax *dax = NULL;
		struct ndctl_pfn *pfn;
		const char *devname;

		if ((dax = ndctl_namespace_get_dax(ndns))) {
			if (pmem2_type == PMEM2_FTYPE_REG ||
			    pmem2_type == PMEM2_FTYPE_DIR)
				continue;

			ASSERTeq(pmem2_type, PMEM2_FTYPE_DEVDAX);

			struct daxctl_region *dax_region;
			dax_region = ndctl_dax_get_daxctl_region(dax);
			if (!dax_region) {
				ERR("!cannot find dax region");
				return -1;
			}
			struct daxctl_dev *dev;
			daxctl_dev_foreach(dax_region, dev) {
				devname = daxctl_dev_get_devname(dev);
				int ret = badblocks_match_devdax(st, devname);
				if (ret < 0)
					return ret;

				if (ret) {
					*pregion = region;
					if (pndns)
						*pndns = ndns;

					return 0;
				}
			}

		} else {
			if (pmem2_type == PMEM2_FTYPE_DEVDAX)
				continue;

			ASSERT(pmem2_type == PMEM2_FTYPE_REG ||
				pmem2_type == PMEM2_FTYPE_DIR);

			if ((btt = ndctl_namespace_get_btt(ndns))) {
				devname = ndctl_btt_get_block_device(btt);
			} else if ((pfn = ndctl_namespace_get_pfn(ndns))) {
				devname = ndctl_pfn_get_block_device(pfn);
			} else {
				devname =
					ndctl_namespace_get_block_device(ndns);
			}

			int ret = badblocks_match_fsdax(st, devname);
			if (ret < 0)
				return ret;

			if (ret) {
				*pregion = region;
				if (pndns)
					*pndns = ndns;

				return 0;
			}
		}
	}

	LOG(10, "did not found any matching device");

	return 0;
}

/*
 * badblocks_get_namespace_bounds -- (internal) returns the bounds
 *                                 (offset and size) of the given namespace
 *                                 relative to the beginning of its region
 */
static int
badblocks_get_namespace_bounds(struct ndctl_region *region,
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
		if (*ns_offset == ULLONG_MAX) {
			ERR("!(pfn) cannot read offset of the namespace");
			return -1;
		}

		*ns_size = ndctl_pfn_get_size(pfn);
		if (*ns_size == ULLONG_MAX) {
			ERR("!(pfn) cannot read size of the namespace");
			return -1;
		}

		LOG(10, "(pfn) ns_offset 0x%llx ns_size %llu",
			*ns_offset, *ns_size);
	} else if (dax) {
		*ns_offset = ndctl_dax_get_resource(dax);
		if (*ns_offset == ULLONG_MAX) {
			ERR("!(dax) cannot read offset of the namespace");
			return -1;
		}

		*ns_size = ndctl_dax_get_size(dax);
		if (*ns_size == ULLONG_MAX) {
			ERR("!(dax) cannot read size of the namespace");
			return -1;
		}

		LOG(10, "(dax) ns_offset 0x%llx ns_size %llu",
			*ns_offset, *ns_size);
	} else { /* raw or btt */
		*ns_offset = ndctl_namespace_get_resource(ndns);
		if (*ns_offset == ULLONG_MAX) {
			ERR("!(raw/btt) cannot read offset of the namespace");
			return -1;
		}

		*ns_size = ndctl_namespace_get_size(ndns);
		if (*ns_size == ULLONG_MAX) {
			ERR("!(raw/btt) cannot read size of the namespace");
			return -1;
		}

		LOG(10, "(raw/btt) ns_offset 0x%llx ns_size %llu",
			*ns_offset, *ns_size);
	}

	unsigned long long region_offset = ndctl_region_get_resource(region);
	if (region_offset == ULLONG_MAX) {
		ERR("!cannot read offset of the region");
		return -1;
	}

	LOG(10, "region_offset 0x%llx", region_offset);
	*ns_offset -= region_offset;

	return 0;
}

/*
 * badblocks_get_badblocks_by_region -- (internal) returns bad blocks
 *                                    in the given namespace using the
 *                                    universal region interface.
 *
 * This function works for all types of namespaces, but requires read access to
 * privileged device information.
 */
static int
badblocks_get_badblocks_by_region(struct ndctl_region *region,
				struct ndctl_namespace *ndns,
				struct badblocks *bbs)
{
	LOG(3, "region %p, namespace %p", region, ndns);

	ASSERTne(bbs, NULL);

	unsigned long long ns_beg, ns_size, ns_end;
	unsigned long long bb_beg, bb_end;
	unsigned long long beg, end;

	VEC(bbsvec, struct bad_block) bbv = VEC_INITIALIZER;

	bbs->ns_resource = 0;
	bbs->bb_cnt = 0;
	bbs->bbv = NULL;

	if (badblocks_get_namespace_bounds(region, ndns, &ns_beg, &ns_size)) {
		LOG(1, "cannot read namespace's bounds");
		return -1;
	}

	ns_end = ns_beg + ns_size - 1;

	LOG(10, "namespace: begin %llu, end %llu size %llu (in 512B sectors)",
		B2SEC(ns_beg), B2SEC(ns_end + 1) - 1, B2SEC(ns_size));

	struct badblock *bb;
	ndctl_region_badblock_foreach(region, bb) {
		/*
		 * libndctl returns offset and length of a bad block
		 * both expressed in 512B sectors and offset is relative
		 * to the beginning of the region.
		 */
		bb_beg = SEC2B(bb->offset);
		bb_end = bb_beg + SEC2B(bb->len) - 1;

		LOG(10,
			"region bad block: begin %llu end %llu length %u (in 512B sectors)",
			bb->offset, bb->offset + bb->len - 1, bb->len);

		if (bb_beg > ns_end || ns_beg > bb_end)
			continue;

		beg = (bb_beg > ns_beg) ? bb_beg : ns_beg;
		end = (bb_end < ns_end) ? bb_end : ns_end;

		/*
		 * Form a new bad block structure with offset and length
		 * expressed in bytes and offset relative to the beginning
		 * of the namespace.
		 */
		struct bad_block bbn;
		bbn.offset = beg - ns_beg;
		bbn.length = (unsigned)(end - beg + 1);
		bbn.nhealthy = NO_HEALTHY_REPLICA; /* unknown healthy replica */

		/* add the new bad block to the vector */
		if (VEC_PUSH_BACK(&bbv, bbn)) {
			VEC_DELETE(&bbv);
			return -1;
		}

		LOG(4,
			"namespace bad block: begin %llu end %llu length %llu (in 512B sectors)",
			B2SEC(beg - ns_beg), B2SEC(end - ns_beg),
			B2SEC(end - beg) + 1);
	}

	bbs->bb_cnt = (unsigned)VEC_SIZE(&bbv);
	bbs->bbv = VEC_ARR(&bbv);
	bbs->ns_resource = ns_beg + ndctl_region_get_resource(region);

	LOG(4, "number of bad blocks detected: %u", bbs->bb_cnt);

	return 0;
}

/*
 * badblocks_get_badblocks_by_namespace -- (internal) returns bad blocks
 *                                    in the given namespace using the
 *                                    block device badblocks interface.
 *
 * This function works only for fsdax, but does not require any special
 * permissions.
 */
static int
badblocks_get_badblocks_by_namespace(struct ndctl_namespace *ndns,
					struct badblocks *bbs)
{
	ASSERTeq(ndctl_namespace_get_mode(ndns), NDCTL_NS_MODE_FSDAX);

	VEC(bbsvec, struct bad_block) bbv = VEC_INITIALIZER;
	struct badblock *bb;
	ndctl_namespace_badblock_foreach(ndns, bb) {
		struct bad_block bbn;
		bbn.offset = SEC2B(bb->offset);
		bbn.length = (unsigned)SEC2B(bb->len);
		bbn.nhealthy = NO_HEALTHY_REPLICA; /* unknown healthy replica */
		if (VEC_PUSH_BACK(&bbv, bbn)) {
			VEC_DELETE(&bbv);
			return -1;
		}
	}

	bbs->bb_cnt = (unsigned)VEC_SIZE(&bbv);
	bbs->bbv = VEC_ARR(&bbv);
	bbs->ns_resource = 0;

	return 0;
}

/*
 * badblocks_get_badblocks -- (internal) returns bad blocks in the given
 *                            namespace, using the least privileged path.
 */
static int
badblocks_get_badblocks(struct ndctl_region *region,
			struct ndctl_namespace *ndns,
			struct badblocks *bbs)
{
	/*
	 * Only the new NDCTL versions have the namespace badblock iterator,
	 * when compiled with older versions, the library needs to rely on the
	 * old region interface.
	 */
	if (ndctl_namespace_get_mode(ndns) == NDCTL_NS_MODE_FSDAX)
		return badblocks_get_badblocks_by_namespace(ndns, bbs);

	return badblocks_get_badblocks_by_region(region, ndns, bbs);
}

/*
 * badblocks_files_namespace_bus -- (internal) returns bus where the given
 *                                file is located
 */
static int
badblocks_files_namespace_bus(struct ndctl_ctx *ctx,
				const char *path,
				struct ndctl_bus **pbus)
{
	LOG(3, "ctx %p path %s pbus %p", ctx, path, pbus);

	ASSERTne(pbus, NULL);

	struct ndctl_region *region;
	struct ndctl_namespace *ndns;

	os_stat_t st;

	if (os_stat(path, &st)) {
		ERR("!stat %s", path);
		return -1;
	}

	int rv = badblocks_region_namespace(ctx, &st, &region, &ndns);
	if (rv) {
		LOG(1, "getting region and namespace failed");
		return -1;
	}

	if (!region) {
		ERR("region unknown");
		return -1;
	}

	*pbus = ndctl_region_get_bus(region);

	return 0;
}

/*
 * badblocks_files_namespace_badblocks_bus -- (internal) returns badblocks
 *                                          in the namespace where the given
 *                                          file is located
 *                                          (optionally returns also the bus)
 */
static int
badblocks_files_namespace_badblocks_bus(struct ndctl_ctx *ctx,
					const char *path,
					struct ndctl_bus **pbus,
					struct badblocks *bbs)
{
	LOG(3, "ctx %p path %s pbus %p badblocks %p", ctx, path, pbus, bbs);

	struct ndctl_region *region;
	struct ndctl_namespace *ndns;

	os_stat_t st;

	if (os_stat(path, &st)) {
		ERR("!stat %s", path);
		return -1;
	}

	int rv = badblocks_region_namespace(ctx, &st, &region, &ndns);
	if (rv) {
		LOG(1, "getting region and namespace failed");
		return -1;
	}

	memset(bbs, 0, sizeof(*bbs));

	if (region == NULL || ndns == NULL)
		return 0;

	if (pbus)
		*pbus = ndctl_region_get_bus(region);

	return badblocks_get_badblocks(region, ndns, bbs);
}

/*
 * badblocks_files_namespace_badblocks -- returns badblocks in the namespace
 *                                      where the given file is located
 */
int
badblocks_files_namespace_badblocks(const char *path, struct badblocks *bbs)
{
	LOG(3, "path %s", path);

	struct ndctl_ctx *ctx;

	if (ndctl_new(&ctx)) {
		ERR("!ndctl_new");
		return -1;
	}

	int ret = badblocks_files_namespace_badblocks_bus(ctx, path, NULL, bbs);

	ndctl_unref(ctx);

	return ret;
}

/*
 * badblocks_devdax_clear_one_badblock -- (internal) clear one bad block
 *                                      in the dax device
 */
static int
badblocks_devdax_clear_one_badblock(struct ndctl_bus *bus,
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

	struct ndctl_range range;
	if (ndctl_cmd_ars_cap_get_range(cmd_ars_cap, &range)) {
		ERR("failed to get ars_cap range\n");
		goto out_ars_cap;
	}

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
out_ars_cap:
	ndctl_cmd_unref(cmd_ars_cap);

	return ret;
}

/*
 * badblocks_devdax_clear_badblocks -- clear the given bad blocks in the dax
 *                                  device (or all of them if 'pbbs' is not set)
 */
int
badblocks_devdax_clear_badblocks(const char *path, struct badblocks *pbbs)
{
	LOG(3, "path %s badblocks %p", path, pbbs);

	struct ndctl_ctx *ctx;
	struct ndctl_bus *bus;
	int ret = -1;

	if (ndctl_new(&ctx)) {
		ERR("!ndctl_new");
		return -1;
	}

	struct badblocks *bbs = badblocks_new();
	if (bbs == NULL)
		goto exit_free_all;

	if (pbbs) {
		ret = badblocks_files_namespace_bus(ctx, path, &bus);
		if (ret) {
			LOG(1, "getting bad blocks' bus failed -- %s", path);
			goto exit_free_all;
		}
		badblocks_delete(bbs);
		bbs = pbbs;
	} else {
		ret = badblocks_files_namespace_badblocks_bus(ctx, path, &bus,
									bbs);
		if (ret) {
			LOG(1, "getting bad blocks for the file failed -- %s",
				path);
			goto exit_free_all;
		}
	}

	if (bbs->bb_cnt == 0 || bbs->bbv == NULL) /* OK - no bad blocks found */
		goto exit_free_all;

	LOG(4, "clearing %u bad block(s)...", bbs->bb_cnt);

	unsigned b;
	for (b = 0; b < bbs->bb_cnt; b++) {
		LOG(4,
			"clearing bad block: offset %llu length %u (in 512B sectors)",
			B2SEC(bbs->bbv[b].offset), B2SEC(bbs->bbv[b].length));

		ret = badblocks_devdax_clear_one_badblock(bus,
					bbs->bbv[b].offset + bbs->ns_resource,
					bbs->bbv[b].length);
		if (ret) {
			LOG(1,
				"failed to clear bad block: offset %llu length %u (in 512B sectors)",
				B2SEC(bbs->bbv[b].offset),
				B2SEC(bbs->bbv[b].length));
			goto exit_free_all;
		}
	}

exit_free_all:
	if (!pbbs)
		badblocks_delete(bbs);

	ndctl_unref(ctx);

	return ret;
}

/*
 * badblocks_devdax_clear_badblocks_all -- clear all bad blocks
 *                                         in the dax device
 */
int
badblocks_devdax_clear_badblocks_all(const char *path)
{
	LOG(3, "path %s", path);

	return badblocks_devdax_clear_badblocks(path, NULL);
}

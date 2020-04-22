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

#include "libpmem2.h"
#include "pmem2_utils.h"
#include "source.h"
#include "ndctl_region_namespace.h"

#include "file.h"
#include "out.h"
#include "os.h"
#include "badblocks.h"
#include "os_badblock.h"
#include "set_badblocks.h"
#include "vec.h"
#include "extent.h"

typedef int pmem2_badblock_next_type(
		struct pmem2_badblock_context *bbctx,
		struct pmem2_badblock *bb);

typedef void *pmem2_badblock_get_next_type(
		struct pmem2_badblock_context *bbctx);

struct pmem2_badblock_context {
	/* file descriptor */
	int fd;

	/* pmem2 file type */
	enum pmem2_file_type file_type;

	/* ndctl context */
	struct ndctl_ctx *ctx;

	/*
	 * Function pointer to:
	 * - pmem2_badblock_next_namespace() or
	 * - pmem2_badblock_next_region()
	 */
	pmem2_badblock_next_type *pmem2_badblock_next_func;

	/*
	 * Function pointer to:
	 * - pmem2_namespace_get_first_badblock() or
	 * - pmem2_namespace_get_next_badblock() or
	 * - pmem2_region_get_first_badblock() or
	 * - pmem2_region_get_next_badblock()
	 */
	pmem2_badblock_get_next_type *pmem2_badblock_get_next_func;

	/* needed only by the ndctl namespace badblock iterator */
	struct ndctl_namespace *ndns;

	/* needed only by the ndctl region badblock iterator */
	struct {
		struct ndctl_bus *bus;
		struct ndctl_region *region;
		unsigned long long ns_res; /* address of the namespace */
		unsigned long long ns_beg; /* the begining of the namespace */
		unsigned long long ns_end; /* the end of the namespace */
	} rgn;

	/* file's extents */
	struct extents *exts;
	unsigned first_extent;
	struct pmem2_badblock last_bb;
};

/* forward declarations */
static int pmem2_badblock_next_namespace(
		struct pmem2_badblock_context *bbctx,
		struct pmem2_badblock *bb);
static int pmem2_badblock_next_region(
		struct pmem2_badblock_context *bbctx,
		struct pmem2_badblock *bb);
static void *pmem2_namespace_get_first_badblock(
		struct pmem2_badblock_context *bbctx);
static void *pmem2_region_get_first_badblock(
		struct pmem2_badblock_context *bbctx);

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
			ERR("(pfn) cannot read offset of the namespace");
			return PMEM2_E_CANNOT_READ_BOUNDS;
		}

		*ns_size = ndctl_pfn_get_size(pfn);
		if (*ns_size == ULLONG_MAX) {
			ERR("(pfn) cannot read size of the namespace");
			return PMEM2_E_CANNOT_READ_BOUNDS;
		}

		LOG(10, "(pfn) ns_offset 0x%llx ns_size %llu",
			*ns_offset, *ns_size);
	} else if (dax) {
		*ns_offset = ndctl_dax_get_resource(dax);
		if (*ns_offset == ULLONG_MAX) {
			ERR("(dax) cannot read offset of the namespace");
			return PMEM2_E_CANNOT_READ_BOUNDS;
		}

		*ns_size = ndctl_dax_get_size(dax);
		if (*ns_size == ULLONG_MAX) {
			ERR("(dax) cannot read size of the namespace");
			return PMEM2_E_CANNOT_READ_BOUNDS;
		}

		LOG(10, "(dax) ns_offset 0x%llx ns_size %llu",
			*ns_offset, *ns_size);
	} else { /* raw or btt */
		*ns_offset = ndctl_namespace_get_resource(ndns);
		if (*ns_offset == ULLONG_MAX) {
			ERR("(raw/btt) cannot read offset of the namespace");
			return PMEM2_E_CANNOT_READ_BOUNDS;
		}

		*ns_size = ndctl_namespace_get_size(ndns);
		if (*ns_size == ULLONG_MAX) {
			ERR("(raw/btt) cannot read size of the namespace");
			return PMEM2_E_CANNOT_READ_BOUNDS;
		}

		LOG(10, "(raw/btt) ns_offset 0x%llx ns_size %llu",
			*ns_offset, *ns_size);
	}

	unsigned long long region_offset = ndctl_region_get_resource(region);
	if (region_offset == ULLONG_MAX) {
		ERR("!cannot read offset of the region");
		return PMEM2_E_ERRNO;
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

	int rv = ndctl_region_namespace(ctx, &st, &region, &ndns);
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

	int rv = ndctl_region_namespace(ctx, &st, &region, &ndns);
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
 * badblocks_devdax_clear_badblocks -- (internal) clear the given bad blocks
 *                                     in the dax device (or all of them
 *                                     if 'pbbs' is not set)
 */
static int
badblocks_devdax_clear_badblocks(const char *path, struct badblocks *pbbs)
{
	LOG(3, "path %s badblocks %p", path, pbbs);

	struct ndctl_ctx *ctx;
	struct ndctl_bus *bus = NULL;
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

	ASSERTne(bus, NULL);

	LOG(4, "clearing %u bad block(s)...", bbs->bb_cnt);

	unsigned b;
	for (b = 0; b < bbs->bb_cnt; b++) {
		LOG(4,
			"clearing bad block: offset %zu length %zu (in 512B sectors)",
			B2SEC(bbs->bbv[b].offset), B2SEC(bbs->bbv[b].length));

		ret = badblocks_devdax_clear_one_badblock(bus,
					bbs->bbv[b].offset + bbs->ns_resource,
					bbs->bbv[b].length);
		if (ret) {
			LOG(1,
				"failed to clear bad block: offset %zu length %zu (in 512B sectors)",
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

/*
 * badblocks_get -- returns 0 and bad blocks in the 'bbs' array
 *                  (that has to be pre-allocated)
 *                  or -1 in case of an error
 */
int
badblocks_get(const char *file, struct badblocks *bbs)
{
	LOG(3, "file %s badblocks %p", file, bbs);

	ASSERTne(bbs, NULL);

	struct pmem2_source *src;
	struct pmem2_badblock_context *bbctx;
	struct pmem2_badblock bb;
	int bb_found = -1; /* -1 means an error */
	int ret;

	VEC(bbsvec, struct bad_block) bbv = VEC_INITIALIZER;
	memset(bbs, 0, sizeof(*bbs));

	int fd = os_open(file, O_RDONLY);
	if (fd == -1) {
		ERR("!open %s", file);
		return -1;
	}

	ret = pmem2_source_from_fd(&src, fd);
	if (ret)
		goto exit_close;

	ret = pmem2_badblock_context_new(src, &bbctx);
	if (ret)
		goto exit_delete_source;

	bb_found = 0;
	while ((pmem2_badblock_next(bbctx, &bb)) == 0) {
		bb_found++;
		/*
		 * Form a new bad block structure with offset and length
		 * expressed in bytes and offset relative
		 * to the beginning of the file.
		 */
		struct bad_block bbn;
		bbn.offset = bb.offset;
		bbn.length = bb.length;
		/* unknown healthy replica */
		bbn.nhealthy = NO_HEALTHY_REPLICA;

		/* add the new bad block to the vector */
		if (VEC_PUSH_BACK(&bbv, bbn)) {
			VEC_DELETE(&bbv);
			bb_found = -1;
			Free(bbs->bbv);
			bbs->bbv = NULL;
			bbs->bb_cnt = 0;
		}
	}

	if (bb_found > 0) {
		bbs->bbv = VEC_ARR(&bbv);
		bbs->bb_cnt = (unsigned)VEC_SIZE(&bbv);

		/*
		 * XXX - this is a temporary solution.
		 * The 'ns_resource' field and whole this assignment
		 * will be removed, when pmem2_badblock_clear()
		 * is added.
		 */
		bbs->ns_resource = bbctx->rgn.ns_res;

		LOG(10, "number of bad blocks detected: %u", bbs->bb_cnt);

		/* sanity check */
		ASSERTeq((unsigned)bb_found, bbs->bb_cnt);
	}

	pmem2_badblock_context_delete(&bbctx);

exit_delete_source:
	pmem2_source_delete(&src);

exit_close:
	if (fd != -1)
		os_close(fd);

	if (ret && bb_found == -1)
		errno = pmem2_err_to_errno(ret);

	return (bb_found >= 0) ? 0 : -1;
}

/*
 * badblocks_count -- returns number of bad blocks in the file
 *                    or -1 in case of an error
 */
long
badblocks_count(const char *file)
{
	LOG(3, "file %s", file);

	struct badblocks *bbs = badblocks_new();
	if (bbs == NULL)
		return -1;

	int ret = badblocks_get(file, bbs);

	long count = (ret == 0) ? (long)bbs->bb_cnt : -1;

	badblocks_delete(bbs);

	return count;
}

/*
 * badblocks_clear_file -- clear the given bad blocks in the regular file
 *                            (not in a dax device)
 */
static int
badblocks_clear_file(const char *file, struct badblocks *bbs)
{
	LOG(3, "file %s badblocks %p", file, bbs);

	ASSERTne(bbs, NULL);

	int ret = 0;
	int fd;

	if ((fd = os_open(file, O_RDWR)) < 0) {
		ERR("!open: %s", file);
		return -1;
	}

	for (unsigned b = 0; b < bbs->bb_cnt; b++) {
		off_t offset = (off_t)bbs->bbv[b].offset;
		off_t length = (off_t)bbs->bbv[b].length;

		LOG(10,
			"clearing bad block: logical offset %li length %li (in 512B sectors) -- '%s'",
			B2SEC(offset), B2SEC(length), file);

		/* deallocate bad blocks */
		if (fallocate(fd, FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE,
				offset, length)) {
			ERR("!fallocate");
			ret = -1;
			break;
		}

		/* allocate new blocks */
		if (fallocate(fd, FALLOC_FL_KEEP_SIZE, offset, length)) {
			ERR("!fallocate");
			ret = -1;
			break;
		}
	}

	os_close(fd);

	return ret;
}

/*
 * badblocks_clear -- clears the given bad blocks in a file
 *                       (regular file or dax device)
 */
int
badblocks_clear(const char *file, struct badblocks *bbs)
{
	LOG(3, "file %s badblocks %p", file, bbs);

	ASSERTne(bbs, NULL);

	struct pmem2_source src;
	struct pmem2_badblock_context *bbctx;
	struct pmem2_badblock bb;
	int ret = -1;

	src.fd = os_open(file, O_RDWR);
	if (src.fd == -1) {
		ERR("!open %s", file);
		return -1;
	}

	ret = pmem2_badblock_context_new(&src, &bbctx);
	if (ret) {
		LOG(1, "pmem2_badblock_context_new failed -- %s", file);
		goto exit_close;
	}

	if (bbctx->rgn.ns_res != bbs->ns_resource) {
		ERR("address of the namespace does not match -- %s", file);
		goto exit_delete_ctx;
	}

	for (unsigned b = 0; b < bbs->bb_cnt; b++) {
		bb.offset = bbs->bbv[b].offset;
		bb.length = bbs->bbv[b].length;
		ret = pmem2_badblock_clear(bbctx, &bb);
		if (ret) {
			LOG(1, "pmem2_badblock_clear -- %s", file);
			goto exit_delete_ctx;
		}
	}

exit_delete_ctx:
	pmem2_badblock_context_delete(&bbctx);

exit_close:
	os_close(src.fd);

	if (ret) {
		errno = pmem2_err_to_errno(ret);
		ret = -1;
	}

	return ret;
}

/*
 * badblocks_clear_all -- clears all bad blocks in a file
 *                           (regular file or dax device)
 */
int
badblocks_clear_all(const char *file)
{
	LOG(3, "file %s", file);

	os_stat_t st;
	if (os_stat(file, &st) < 0) {
		ERR("!stat %s", file);
		return -1;
	}

	enum pmem2_file_type pmem2_type;

	int ret = pmem2_get_type_from_stat(&st, &pmem2_type);
	if (ret) {
		errno = pmem2_err_to_errno(ret);
		return -1;
	}

	if (pmem2_type == PMEM2_FTYPE_DEVDAX)
		return badblocks_devdax_clear_badblocks_all(file);

	if (pmem2_type != PMEM2_FTYPE_REG)
		/* unsupported file type */
		return -1;

	struct badblocks *bbs = badblocks_new();
	if (bbs == NULL)
		return -1;

	ret = badblocks_get(file, bbs);
	if (ret) {
		LOG(1, "checking bad blocks in the file failed -- '%s'", file);
		goto error_free_all;
	}

	if (bbs->bb_cnt > 0) {
		ret = badblocks_clear_file(file, bbs);
		if (ret < 0) {
			LOG(1, "clearing bad blocks in the file failed -- '%s'",
				file);
			goto error_free_all;
		}
	}

error_free_all:
	badblocks_delete(bbs);

	return ret;
}

/*
 * pmem2_badblock_context_new -- allocate and create a new bad block context
 */
int
pmem2_badblock_context_new(const struct pmem2_source *src,
	struct pmem2_badblock_context **bbctx)
{
	LOG(3, "src %p bbctx %p", src, bbctx);

	ASSERTne(bbctx, NULL);

	struct ndctl_ctx *ctx;
	struct ndctl_region *region;
	struct ndctl_namespace *ndns;
	struct pmem2_badblock_context *tbbctx = NULL;
	enum pmem2_file_type pmem2_type;
	int ret = PMEM2_E_UNKNOWN;
	os_stat_t st;

	*bbctx = NULL;

	if (ndctl_new(&ctx)) {
		ERR("!ndctl_new");
		return PMEM2_E_ERRNO;
	}

	if (os_fstat(src->fd, &st)) {
		ERR("!fstat %i", src->fd);
		ret = PMEM2_E_ERRNO;
		goto exit_ndctl_unref;
	}

	ret = pmem2_get_type_from_stat(&st, &pmem2_type);
	if (ret)
		goto exit_ndctl_unref;

	ret = ndctl_region_namespace(ctx, &st, &region, &ndns);
	if (ret) {
		LOG(1, "getting region and namespace failed");
		goto exit_ndctl_unref;
	}

	tbbctx = pmem2_zalloc(sizeof(struct pmem2_badblock_context), &ret);
	if (ret)
		goto exit_ndctl_unref;

	tbbctx->fd = src->fd;
	tbbctx->file_type = pmem2_type;
	tbbctx->ctx = ctx;

	if (region == NULL || ndns == NULL) {
		/* did not found any matching device */
		*bbctx = tbbctx;
		return 0;
	}

	if (ndctl_namespace_get_mode(ndns) == NDCTL_NS_MODE_FSDAX) {
		tbbctx->ndns = ndns;
		tbbctx->pmem2_badblock_next_func =
			pmem2_badblock_next_namespace;
		tbbctx->pmem2_badblock_get_next_func =
			pmem2_namespace_get_first_badblock;
	} else {
		unsigned long long ns_beg, ns_size, ns_end;
		ret = badblocks_get_namespace_bounds(
				region, ndns,
				&ns_beg, &ns_size);
		if (ret) {
			LOG(1, "cannot read namespace's bounds");
			goto error_free_all;
		}

		ns_end = ns_beg + ns_size - 1;

		LOG(10,
			"namespace: begin %llu, end %llu size %llu (in 512B sectors)",
			B2SEC(ns_beg), B2SEC(ns_end + 1) - 1, B2SEC(ns_size));

		tbbctx->rgn.bus = ndctl_region_get_bus(region);
		tbbctx->rgn.region = region;
		tbbctx->rgn.ns_beg = ns_beg;
		tbbctx->rgn.ns_end = ns_end;
		tbbctx->rgn.ns_res = ns_beg + ndctl_region_get_resource(region);
		tbbctx->pmem2_badblock_next_func =
			pmem2_badblock_next_region;
		tbbctx->pmem2_badblock_get_next_func =
			pmem2_region_get_first_badblock;
	}

	if (pmem2_type == PMEM2_FTYPE_REG) {
		/* only regular files have extents */
		ret = pmem2_extents_create_get(src->fd, &tbbctx->exts);
		if (ret) {
			LOG(1, "getting extents of fd %i failed", src->fd);
			goto error_free_all;
		}
	}

	/* set the context */
	*bbctx = tbbctx;

	return 0;

error_free_all:
	pmem2_extents_destroy(&tbbctx->exts);
	Free(tbbctx);

exit_ndctl_unref:
	ndctl_unref(ctx);

	return ret;
}

/*
 * pmem2_badblock_context_delete -- delete and free the bad block context
 */
void
pmem2_badblock_context_delete(struct pmem2_badblock_context **bbctx)
{
	LOG(3, "bbctx %p", bbctx);

	ASSERTne(bbctx, NULL);

	struct pmem2_badblock_context *tbbctx = *bbctx;

	pmem2_extents_destroy(&tbbctx->exts);
	ndctl_unref(tbbctx->ctx);
	Free(tbbctx);

	*bbctx = NULL;
}

/*
 * pmem2_namespace_get_next_badblock -- (internal) wrapper for
 *                                      ndctl_namespace_get_next_badblock
 */
static void *
pmem2_namespace_get_next_badblock(struct pmem2_badblock_context *bbctx)
{
	LOG(3, "bbctx %p", bbctx);

	return ndctl_namespace_get_next_badblock(bbctx->ndns);
}

/*
 * pmem2_namespace_get_first_badblock -- (internal) wrapper for
 *                                       ndctl_namespace_get_first_badblock
 */
static void *
pmem2_namespace_get_first_badblock(struct pmem2_badblock_context *bbctx)
{
	LOG(3, "bbctx %p", bbctx);

	bbctx->pmem2_badblock_get_next_func = pmem2_namespace_get_next_badblock;
	return ndctl_namespace_get_first_badblock(bbctx->ndns);
}

/*
 * pmem2_region_get_next_badblock -- (internal) wrapper for
 *                                   ndctl_region_get_next_badblock
 */
static void *
pmem2_region_get_next_badblock(struct pmem2_badblock_context *bbctx)
{
	LOG(3, "bbctx %p", bbctx);

	return ndctl_region_get_next_badblock(bbctx->rgn.region);
}

/*
 * pmem2_region_get_first_badblock -- (internal) wrapper for
 *                                    ndctl_region_get_first_badblock
 */
static void *
pmem2_region_get_first_badblock(struct pmem2_badblock_context *bbctx)
{
	LOG(3, "bbctx %p", bbctx);

	bbctx->pmem2_badblock_get_next_func = pmem2_region_get_next_badblock;
	return ndctl_region_get_first_badblock(bbctx->rgn.region);
}

/*
 * pmem2_badblock_next_namespace -- (internal) version of pmem2_badblock_next()
 *                                  called for ndctl with namespace badblock
 *                                  iterator
 *
 * This function works only for fsdax, but does not require any special
 * permissions.
 */
static int
pmem2_badblock_next_namespace(struct pmem2_badblock_context *bbctx,
				struct pmem2_badblock *bb)
{
	LOG(3, "bbctx %p bb %p", bbctx, bb);

	ASSERTne(bbctx, NULL);
	ASSERTne(bb, NULL);

	struct badblock *bbn;

	bbn = bbctx->pmem2_badblock_get_next_func(bbctx);
	if (bbn == NULL)
		return PMEM2_E_NO_BAD_BLOCK_FOUND;

	/*
	 * libndctl returns offset and length of a bad block
	 * both expressed in 512B sectors. Offset is relative
	 * to the beginning of the namespace.
	 */
	bb->offset = SEC2B(bbn->offset);
	bb->length = SEC2B(bbn->len);

	return 0;
}

/*
 * pmem2_badblock_next_region -- (internal) version of pmem2_badblock_next()
 *                               called for ndctl with region badblock iterator
 *
 * This function works for all types of namespaces, but requires read access to
 * privileged device information.
 */
static int
pmem2_badblock_next_region(struct pmem2_badblock_context *bbctx,
				struct pmem2_badblock *bb)
{
	LOG(3, "bbctx %p bb %p", bbctx, bb);

	ASSERTne(bbctx, NULL);
	ASSERTne(bb, NULL);

	unsigned long long bb_beg, bb_end;
	unsigned long long beg, end;
	struct badblock *bbn;

	unsigned long long ns_beg = bbctx->rgn.ns_beg;
	unsigned long long ns_end = bbctx->rgn.ns_end;

	do {
		bbn = bbctx->pmem2_badblock_get_next_func(bbctx);
		if (bbn == NULL)
			return PMEM2_E_NO_BAD_BLOCK_FOUND;

		LOG(10,
			"region bad block: begin %llu end %llu length %u (in 512B sectors)",
			bbn->offset, bbn->offset + bbn->len - 1, bbn->len);

		/*
		 * libndctl returns offset and length of a bad block
		 * both expressed in 512B sectors. Offset is relative
		 * to the beginning of the region.
		 */
		bb_beg = SEC2B(bbn->offset);
		bb_end = bb_beg + SEC2B(bbn->len) - 1;

	} while (bb_beg > ns_end || ns_beg > bb_end);

	beg = (bb_beg > ns_beg) ? bb_beg : ns_beg;
	end = (bb_end < ns_end) ? bb_end : ns_end;

	/*
	 * Form a new bad block structure with offset and length
	 * expressed in bytes and offset relative to the beginning
	 * of the namespace.
	 */
	bb->offset = beg - ns_beg;
	bb->length = end - beg + 1;

	LOG(4,
		"namespace bad block: begin %llu end %llu length %llu (in 512B sectors)",
		B2SEC(beg - ns_beg), B2SEC(end - ns_beg), B2SEC(end - beg) + 1);

	return 0;
}

/*
 * pmem2_badblock_next -- get the next bad block
 */
int
pmem2_badblock_next(struct pmem2_badblock_context *bbctx,
			struct pmem2_badblock *bb)
{
	LOG(3, "bbctx %p bb %p", bbctx, bb);

	ASSERTne(bbctx, NULL);
	ASSERTne(bb, NULL);

	const struct pmem2_badblock BB_ZERO = {0, 0};
	struct pmem2_badblock bbn = BB_ZERO;
	unsigned long long bb_beg;
	unsigned long long bb_end;
	unsigned long long bb_len;
	unsigned long long bb_off;
	unsigned long long ext_beg;
	unsigned long long ext_end;
	unsigned e;
	int ret;

	if (bbctx->rgn.region == NULL && bbctx->ndns == NULL) {
		/* did not found any matching device */
		*bb = BB_ZERO;
		return PMEM2_E_NO_BAD_BLOCK_FOUND;
	}

	struct extents *exts = bbctx->exts;

	/* DAX devices have no extents */
	if (!exts) {
		ret = bbctx->pmem2_badblock_next_func(bbctx, &bbn);
		*bb = bbn;
		return ret;
	}

	/*
	 * There is at least one extent.
	 * Loop until:
	 * 1) a bad block overlaps with an extent or
	 * 2) there are no more bad blocks.
	 */
	int bb_overlaps_with_extent = 0;
	do {
		if (bbctx->last_bb.length) {
			/*
			 * We have saved the last bad block to check it
			 * with the next extent saved
			 * in bbctx->first_extent.
			 */
			ASSERTne(bbctx->first_extent, 0);
			bbn = bbctx->last_bb;
			bbctx->last_bb.offset = 0;
			bbctx->last_bb.length = 0;
		} else {
			ASSERTeq(bbctx->first_extent, 0);
			/* look for the next bad block */
			ret = bbctx->pmem2_badblock_next_func(bbctx, &bbn);
			if (ret)
				return ret;
		}

		bb_beg = bbn.offset;
		bb_end = bb_beg + bbn.length - 1;

		for (e = bbctx->first_extent;
				e < exts->extents_count;
				e++) {

			ext_beg = exts->extents[e].offset_physical;
			ext_end = ext_beg + exts->extents[e].length - 1;

			/* check if the bad block overlaps with the extent */
			if (bb_beg <= ext_end && ext_beg <= bb_end) {
				/* bad block overlaps with the extent */
				bb_overlaps_with_extent = 1;

				if (bb_end > ext_end &&
				    e + 1 < exts->extents_count) {
					/*
					 * The bad block is longer than
					 * the extent and there are
					 * more extents.
					 * Save the current bad block
					 * to check it with the next extent.
					 */
					bbctx->first_extent = e + 1;
					bbctx->last_bb = bbn;
				} else {
					/*
					 * All extents were checked
					 * with the current bad block.
					 */
					bbctx->first_extent = 0;
					bbctx->last_bb.length = 0;
					bbctx->last_bb.offset = 0;
				}
				break;
			}
		}

		/* check all extents with the next bad block */
		if (bb_overlaps_with_extent == 0) {
			bbctx->first_extent = 0;
			bbctx->last_bb.length = 0;
			bbctx->last_bb.offset = 0;
		}

	} while (bb_overlaps_with_extent == 0);

	/* bad block overlaps with an extent */

	bb_beg = (bb_beg > ext_beg) ? bb_beg : ext_beg;
	bb_end = (bb_end < ext_end) ? bb_end : ext_end;
	bb_len = bb_end - bb_beg + 1;
	bb_off = bb_beg + exts->extents[e].offset_logical
			- exts->extents[e].offset_physical;

	LOG(10, "bad block found: physical offset: %llu, length: %llu",
		bb_beg, bb_len);

	/* make sure the offset is block-aligned */
	unsigned long long not_block_aligned = bb_off & (exts->blksize - 1);
	if (not_block_aligned) {
		bb_off -= not_block_aligned;
		bb_len += not_block_aligned;
	}

	/* make sure the length is block-aligned */
	bb_len = ALIGN_UP(bb_len, exts->blksize);

	LOG(4, "bad block found: logical offset: %llu, length: %llu",
		bb_off, bb_len);

	/*
	 * Return the bad block with offset and length
	 * expressed in bytes and offset relative
	 * to the beginning of the file.
	 */
	bb->offset = bb_off;
	bb->length = bb_len;

	return 0;
}

/*
 * pmem2_badblock_clear_fsdax -- (internal) clear one bad block
 *                               in a FSDAX device
 */
static int
pmem2_badblock_clear_fsdax(int fd, const struct pmem2_badblock *bb)
{
	LOG(3, "fd %i badblock %p", fd, bb);

	ASSERTne(bb, NULL);

	LOG(10,
		"clearing a bad block: fd %i logical offset %zu length %zu (in 512B sectors)",
		fd, B2SEC(bb->offset), B2SEC(bb->length));

	off_t offset = (off_t)bb->offset;
	off_t length = (off_t)bb->length;

	/* deallocate bad blocks */
	if (fallocate(fd, FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE,
			offset, length)) {
		ERR("!fallocate");
		return PMEM2_E_ERRNO;
	}

	/* allocate new blocks */
	if (fallocate(fd, FALLOC_FL_KEEP_SIZE, offset, length)) {
		ERR("!fallocate");
		return PMEM2_E_ERRNO;
	}

	return 0;
}

/*
 * pmem2_badblock_clear_devdax -- (internal) clear one bad block
 *                                in a DAX device
 */
static int
pmem2_badblock_clear_devdax(const struct pmem2_badblock_context *bbctx,
				const struct pmem2_badblock *bb)
{
	LOG(3, "bbctx %p bb %p", bbctx, bb);

	ASSERTne(bb, NULL);
	ASSERTne(bbctx, NULL);
	ASSERTne(bbctx->rgn.bus, NULL);
	ASSERTne(bbctx->rgn.ns_res, 0);

	LOG(4,
		"clearing a bad block: offset %zu length %zu (in 512B sectors)",
		B2SEC(bb->offset), B2SEC(bb->length));

	int ret = badblocks_devdax_clear_one_badblock(bbctx->rgn.bus,
				bb->offset + bbctx->rgn.ns_res,
				bb->length);
	if (ret) {
		LOG(1,
			"failed to clear a bad block: offset %zu length %zu (in 512B sectors)",
			B2SEC(bb->offset),
			B2SEC(bb->length));
		return ret;
	}

	return 0;
}

/*
 * pmem2_badblock_clear -- clear one bad block
 */
int
pmem2_badblock_clear(struct pmem2_badblock_context *bbctx,
			const struct pmem2_badblock *bb)
{
	LOG(3, "bbctx %p badblock %p", bbctx, bb);

	ASSERTne(bbctx, NULL);
	ASSERTne(bb, NULL);

	if (bbctx->file_type == PMEM2_FTYPE_DEVDAX)
		return pmem2_badblock_clear_devdax(bbctx, bb);

	if (bbctx->file_type == PMEM2_FTYPE_REG)
		return pmem2_badblock_clear_fsdax(bbctx->fd, bb);

	return PMEM2_E_INVALID_FILE_TYPE;
}

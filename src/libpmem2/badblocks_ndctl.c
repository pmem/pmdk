// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2017-2021, Intel Corporation */

/*
 * badblocks_ndctl.c -- implementation of DIMMs API based on the ndctl library
 */
#define _GNU_SOURCE

#include <sys/types.h>
#include <libgen.h>
#include <linux/falloc.h>
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
#include "region_namespace_ndctl.h"

#include "file.h"
#include "out.h"
#include "badblocks.h"
#include "set_badblocks.h"
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
		unsigned long long ns_beg; /* the beginning of the namespace */
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
 *                                   (offset and size) of the given namespace
 *                                   relative to the beginning of its region
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
 * badblocks_devdax_clear_one_badblock -- (internal) clear one bad block
 *                                        in the dax device
 */
static int
badblocks_devdax_clear_one_badblock(struct ndctl_bus *bus,
				unsigned long long address,
				unsigned long long length)
{
	LOG(3, "bus %p address 0x%llx length %llu (bytes)",
		bus, address, length);

	int ret;

	struct ndctl_cmd *cmd_ars_cap = ndctl_bus_cmd_new_ars_cap(bus,
							address, length);
	if (cmd_ars_cap == NULL) {
		ERR("ndctl_bus_cmd_new_ars_cap() failed (bus '%s')",
			ndctl_bus_get_provider(bus));
		return PMEM2_E_ERRNO;
	}

	ret = ndctl_cmd_submit(cmd_ars_cap);
	if (ret) {
		ERR("ndctl_cmd_submit() failed (bus '%s')",
			ndctl_bus_get_provider(bus));
		/* ndctl_cmd_submit() returns -errno */
		goto out_ars_cap;
	}

	struct ndctl_range range;
	ret = ndctl_cmd_ars_cap_get_range(cmd_ars_cap, &range);
	if (ret) {
		ERR("ndctl_cmd_ars_cap_get_range() failed");
		/* ndctl_cmd_ars_cap_get_range() returns -errno */
		goto out_ars_cap;
	}

	struct ndctl_cmd *cmd_clear_error = ndctl_bus_cmd_new_clear_error(
		range.address, range.length, cmd_ars_cap);

	ret = ndctl_cmd_submit(cmd_clear_error);
	if (ret) {
		ERR("ndctl_cmd_submit() failed (bus '%s')",
			ndctl_bus_get_provider(bus));
		/* ndctl_cmd_submit() returns -errno */
		goto out_clear_error;
	}

	size_t cleared = ndctl_cmd_clear_error_get_cleared(cmd_clear_error);

	LOG(4, "cleared %zu out of %llu bad blocks", cleared, length);

	ASSERT(cleared <= length);

	if (cleared < length) {
		ERR("failed to clear %llu out of %llu bad blocks",
			length - cleared, length);
		errno = ENXIO; /* ndctl handles such error in this way */
		ret = PMEM2_E_ERRNO;
	} else {
		ret = 0;
	}

out_clear_error:
	ndctl_cmd_unref(cmd_clear_error);
out_ars_cap:
	ndctl_cmd_unref(cmd_ars_cap);

	return ret;
}

/*
 * pmem2_badblock_context_new -- allocate and create a new bad block context
 */
int
pmem2_badblock_context_new(struct pmem2_badblock_context **bbctx,
	const struct pmem2_source *src)
{
	LOG(3, "src %p bbctx %p", src, bbctx);
	PMEM2_ERR_CLR();

	ASSERTne(bbctx, NULL);

	if (src->type == PMEM2_SOURCE_ANON) {
		ERR("Anonymous source does not support bad blocks");
		return PMEM2_E_NOSUPP;
	}

	ASSERTeq(src->type, PMEM2_SOURCE_FD);

	struct ndctl_ctx *ctx;
	struct ndctl_region *region;
	struct ndctl_namespace *ndns;
	struct pmem2_badblock_context *tbbctx = NULL;
	enum pmem2_file_type pmem2_type;
	int ret = PMEM2_E_UNKNOWN;
	*bbctx = NULL;

	errno = ndctl_new(&ctx) * (-1);
	if (errno) {
		ERR("!ndctl_new");
		return PMEM2_E_ERRNO;
	}

	pmem2_type = src->value.ftype;

	ret = pmem2_region_namespace(ctx, src, &region, &ndns);
	if (ret) {
		LOG(1, "getting region and namespace failed");
		goto exit_ndctl_unref;
	}

	tbbctx = pmem2_zalloc(sizeof(struct pmem2_badblock_context), &ret);
	if (ret)
		goto exit_ndctl_unref;

	tbbctx->fd = src->value.fd;
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
		ret = pmem2_extents_create_get(src->value.fd, &tbbctx->exts);
		if (ret) {
			LOG(1, "getting extents of fd %i failed",
				src->value.fd);
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
	PMEM2_ERR_CLR();

	ASSERTne(bbctx, NULL);

	if (*bbctx == NULL)
		return;

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
	PMEM2_ERR_CLR();

	ASSERTne(bbctx, NULL);
	ASSERTne(bb, NULL);

	struct pmem2_badblock bbn;
	unsigned long long bb_beg;
	unsigned long long bb_end;
	unsigned long long bb_len;
	unsigned long long bb_off;
	unsigned long long ext_beg = 0; /* placate compiler warnings */
	unsigned long long ext_end = -1ULL;
	unsigned e;
	int ret;

	if (bbctx->rgn.region == NULL && bbctx->ndns == NULL) {
		ERR("Cannot find any matching device, no bad blocks found");
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
	PMEM2_ERR_CLR();

	ASSERTne(bb, NULL);

	LOG(10,
		"clearing a bad block: fd %i logical offset %zu length %zu (in 512B sectors)",
		fd, B2SEC(bb->offset), B2SEC(bb->length));

	/* fallocate() takes offset as the off_t type */
	if (bb->offset > (size_t)INT64_MAX) {
		ERR("bad block's offset is greater than INT64_MAX");
		return PMEM2_E_OFFSET_OUT_OF_RANGE;
	}

	/* fallocate() takes length as the off_t type */
	if (bb->length > (size_t)INT64_MAX) {
		ERR("bad block's length is greater than INT64_MAX");
		return PMEM2_E_LENGTH_OUT_OF_RANGE;
	}

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
	PMEM2_ERR_CLR();

	ASSERTne(bbctx, NULL);
	ASSERTne(bb, NULL);

	if (bbctx->file_type == PMEM2_FTYPE_DEVDAX)
		return pmem2_badblock_clear_devdax(bbctx, bb);

	ASSERTeq(bbctx->file_type, PMEM2_FTYPE_REG);

	return pmem2_badblock_clear_fsdax(bbctx->fd, bb);
}

// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021, Intel Corporation */

/*
 * badblock.c -- implementation of common badblock API
 */

#include <errno.h>
#include <libpmem2.h>

#include "badblock.h"
#include "config.h"
#include "file.h"
#include "libpmemset.h"
#include "out.h"
#include "pmemset.h"
#include "pmemset_utils.h"
#include "source.h"
#include "util.h"

/*
 * pmemset_badblock_to_pmem2_badblock -- convert pmemset badblock into pmem2 one
 */
static struct pmem2_badblock
pmemset_badblock_to_pmem2_badblock(struct pmemset_badblock *bb)
{
	struct pmem2_badblock pmem2_bb;
	pmem2_bb.offset = bb->offset;
	pmem2_bb.length = bb->length;

	return pmem2_bb;
}

/*
 * pmem2_badblock_to_pmemset_badblock -- convert pmem2 badblock into pmemset one
 */
static struct pmemset_badblock
pmem2_badblock_to_pmemset_badblock(struct pmem2_badblock *pmem2_bb)
{
	struct pmemset_badblock bb;
	bb.offset = pmem2_bb->offset;
	bb.length = pmem2_bb->length;

	return bb;
}

/*
 * pmemset_translate_pmem2_badblock_context_new_error -- translate pmem2 errors
 * from pmem2_badblock_context_new function to pmemset errors
 */
static int
pmemset_translate_pmem2_badblock_context_new_error(int ret,
		struct pmemset_source *src)
{
	switch (ret) {
		case 0:
			return 0;
		case PMEM2_E_NOSUPP:
			ERR("bad block handling isn't supported on this OS");
			return PMEMSET_E_BADBLOCK_NOSUPP;
		case PMEM2_E_DAX_REGION_NOT_FOUND:
			ERR("cannot find dax region");
			return PMEMSET_E_DAX_REGION_NOT_FOUND;
		case PMEM2_E_CANNOT_READ_BOUNDS:
			ERR("cannot read offset or size of the namespace of "
					"the source %p", src);
			return PMEMSET_E_CANNOT_READ_BOUNDS;
		case PMEM2_E_INVALID_FILE_TYPE:
			/*
			 * underlying pmem2 sources in pmemset sources are
			 * created only from regular files or character devices,
			 * which are allowed by pmem2_badblock_context_new
			 */
			ASSERT(0);
		default:
			ERR("!pmem2_badblock_context_new");
			return ret;
	}
}

/*
 * typedef forearch callback function invoked on each iteration of badblock
 * contained in the source
 */
typedef int pmemset_bb_foreach_cb(struct pmemset_badblock *bb,
		struct pmemset *set, struct pmemset_source *src);

/*
 * pmemset_badblock_foreach -- invoke callback function for each badblock
 *                             detected in the source and return bb count
 */
static int
pmemset_badblock_foreach(struct pmemset *set, struct pmemset_source *src,
		pmemset_bb_foreach_cb cb, size_t *count)
{
	if (count)
		*count = 0;

	struct pmemset_file *part_file = pmemset_source_get_set_file(src);
	struct pmem2_source *pmem2_src =
			pmemset_file_get_pmem2_source(part_file);

	struct pmem2_badblock_context *bbctx;
	int ret = pmem2_badblock_context_new(&bbctx, pmem2_src);
	ret = pmemset_translate_pmem2_badblock_context_new_error(ret, src);
	if (ret)
		return ret;

	size_t bb_count = 0;
	struct pmem2_badblock pmem2_bb;
	while ((ret = pmem2_badblock_next(bbctx, &pmem2_bb)) == 0) {
		bb_count += 1;

		if (cb) {
			struct pmemset_badblock bb =
				pmem2_badblock_to_pmemset_badblock(&pmem2_bb);
			cb(&bb, set, src);
		}
	}

	pmem2_badblock_context_delete(&bbctx);

	/*
	 * pmem2_badblock_next can only return PMEM2_E_NOSUPP and
	 * PMEM2_E_NO_BAD_BLOCK_FOUND
	 */
	if (ret == PMEM2_E_NOSUPP) {
		ERR("bad block handling isn't supported on this OS");
		return PMEMSET_E_BADBLOCK_NOSUPP;
	}

	if (count)
		*count = bb_count;

	return 0;
}

/*
 * pmemset_badblock_fire_badblock_event -- fire PMEMSET_EVENT_BADBLOCK event
 */
static int
pmemset_badblock_fire_badblock_event(struct pmemset_badblock *bb,
		struct pmemset *set, struct pmemset_source *src)
{
	struct pmemset_event_badblock event;
	event.bb = bb;
	event.src = src;

	struct pmemset_event_context ctx;
	ctx.type = PMEMSET_EVENT_BADBLOCK;
	ctx.data.badblock = event;

	struct pmemset_config *cfg = pmemset_get_config(set);
	return pmemset_config_event_callback(cfg, set, &ctx);
}

/*
 * pmemset_badblock_fire_all_badblocks_cleared_event -- fire
 * PMEMSET_EVENT_ALL_BADBLOCKS_CLEARED event
 */
static int
pmemset_badblock_fire_all_badblocks_cleared_event(struct pmemset *set,
		struct pmemset_source *src)
{
	struct pmemset_event_badblocks_cleared event;
	event.src = src;

	struct pmemset_event_context ctx;
	ctx.type = PMEMSET_EVENT_BADBLOCKS_CLEARED;
	ctx.data.badblocks_cleared = event;

	struct pmemset_config *cfg = pmemset_get_config(set);
	return pmemset_config_event_callback(cfg, set, &ctx);
}

/*
 * pmemset_badblock_clear -- clear badblock from the source
 */
int
pmemset_badblock_clear(struct pmemset_badblock *bb, struct pmemset_source *src)
{
	LOG(3, "bb %p src %p", bb, src);
	PMEMSET_ERR_CLR();

	struct pmemset_file *file = pmemset_source_get_set_file(src);
	struct pmem2_source *pmem2_src = pmemset_file_get_pmem2_source(file);

	struct pmem2_badblock_context *bbctx;
	int ret = pmem2_badblock_context_new(&bbctx, pmem2_src);
	ret = pmemset_translate_pmem2_badblock_context_new_error(ret, src);
	if (ret)
		return ret;

	struct pmem2_badblock pmem2_bb =
			pmemset_badblock_to_pmem2_badblock(bb);

	ret = pmem2_badblock_clear(bbctx, &pmem2_bb);
	pmem2_badblock_context_delete(&bbctx);

	switch (ret) {
		case 0:
			break;
		case PMEM2_E_NOSUPP:
			ERR("bad block handling isn't supported on this OS");
			return PMEMSET_E_BADBLOCK_NOSUPP;
		case PMEM2_E_OFFSET_OUT_OF_RANGE:
			ERR("bad block offset is greater than INT64_MAX");
			return PMEMSET_E_OFFSET_OUT_OF_RANGE;
		case PMEM2_E_LENGTH_OUT_OF_RANGE:
			ERR("bad block length is greater than INT64_MAX");
			return PMEMSET_E_LENGTH_OUT_OF_RANGE;
		default:
			ERR("!pmem2_badblock_clear");
			return ret;
	}

	return ret;
}

/*
 * pmemset_badblock_detect_check_if_cleared -- fire PMEMSET_EVENT_BADBLOCK for
 * each bad block detected. Fire PMEMSET_EVENT_ALL_BADBLOCKS_CLEARED if
 * badblocks were cleared via event callback function.
 */
int
pmemset_badblock_detect_check_if_cleared(struct pmemset *set,
		struct pmemset_source *src)
{
	size_t bb_count;
	int ret = pmemset_badblock_foreach(set, src,
		pmemset_badblock_fire_badblock_event, &bb_count);
	if (ret)
		return ret;

	/* check if user defined callbacks cleared badblocks */
	if (bb_count != 0) {
		ret = pmemset_badblock_foreach(set, src, NULL, &bb_count);
		if (ret)
			return ret;

		if (bb_count != 0) {
			ERR("operation encountered %zu badblocks in "
				"source %p", bb_count, src);
			return PMEMSET_E_IO_FAIL;
		}

		ret = pmemset_badblock_fire_all_badblocks_cleared_event(set,
				src);
		if (ret)
			return ret;
	}

	return 0;
}

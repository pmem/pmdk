/*
 * Copyright 2015-2017, Intel Corporation
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
 * pmalloc.c -- implementation of pmalloc POSIX-like API
 *
 * This is the front-end part of the persistent memory allocator. It uses both
 * transient and persistent representation of the heap to provide memory blocks
 * in a reasonable time and with an acceptable common-case fragmentation.
 */

#include "valgrind_internal.h"
#include "heap.h"
#include "lane.h"
#include "memops.h"
#include "obj.h"
#include "out.h"
#include "palloc.h"
#include "pmalloc.h"
#include "alloc_class.h"

#ifdef DEBUG
/*
 * In order to prevent allocations from inside of a constructor, each lane hold
 * invocation sets the, otherwise unused, runtime part of the lane section
 * to a value that marks an in progress allocation. Likewise, each lane release
 * sets the runtime variable back to NULL.
 *
 * Because this check requires additional hold/release pair for every single
 * allocation, it's done only for debug builds.
 */
#define ALLOC_INPROGRESS_MARK ((void *)0x1)
#endif

/*
 * pmalloc_redo_hold -- acquires allocator lane section and returns a pointer to
 * it's redo log
 */
struct redo_log *
pmalloc_redo_hold(PMEMobjpool *pop)
{
	struct lane_section *lane;
	lane_hold(pop, &lane, LANE_SECTION_ALLOCATOR);

#ifdef DEBUG
	ASSERTeq(lane->runtime, NULL);
	lane->runtime = ALLOC_INPROGRESS_MARK;
#endif

	struct lane_alloc_layout *sec = (void *)lane->layout;
	return sec->redo;
}

/*
 * pmalloc_redo_release -- releases allocator lane section
 */
void
pmalloc_redo_release(PMEMobjpool *pop)
{
#ifdef DEBUG
	/* there's no easier way, I've tried */
	struct lane_section *lane;
	lane_hold(pop, &lane, LANE_SECTION_ALLOCATOR);
	lane->runtime = NULL;
	lane_release(pop);
#endif
	lane_release(pop);
}

/*
 * pmalloc_operation -- higher level wrapper for basic allocator API
 *
 * If successful function returns zero. Otherwise an error number is returned.
 */
int
pmalloc_operation(struct palloc_heap *heap, uint64_t off, uint64_t *dest_off,
	size_t size, palloc_constr constructor, void *arg,
	uint64_t extra_field, uint16_t object_flags, uint16_t class_id,
	struct operation_context *ctx)
{
#ifdef USE_VG_MEMCHECK
	uint64_t tmp;
	if (size && On_valgrind && dest_off == NULL)
		dest_off = &tmp;
#endif

	int ret = palloc_operation(heap, off, dest_off, size, constructor, arg,
			extra_field, object_flags, class_id, ctx);
	if (ret)
		return ret;

	return 0;
}

/*
 * pmalloc -- allocates a new block of memory
 *
 * The pool offset is written persistently into the off variable.
 *
 * If successful function returns zero. Otherwise an error number is returned.
 */
int
pmalloc(PMEMobjpool *pop, uint64_t *off, size_t size,
	uint64_t extra_field, uint16_t object_flags)
{
	struct redo_log *redo = pmalloc_redo_hold(pop);

	struct operation_context ctx;
	operation_init(&ctx, pop, pop->redo, redo);

	int ret = pmalloc_operation(&pop->heap, 0, off, size, NULL, NULL,
		extra_field, object_flags, 0, &ctx);

	pmalloc_redo_release(pop);

	return ret;
}

/*
 * pmalloc_construct -- allocates a new block of memory with a constructor
 *
 * The block offset is written persistently into the off variable, but only
 * after the constructor function has been called.
 *
 * If successful function returns zero. Otherwise an error number is returned.
 */
int
pmalloc_construct(PMEMobjpool *pop, uint64_t *off, size_t size,
	palloc_constr constructor, void *arg,
	uint64_t extra_field, uint16_t object_flags, uint16_t class_id)
{
	struct redo_log *redo = pmalloc_redo_hold(pop);
	struct operation_context ctx;

	operation_init(&ctx, pop, pop->redo, redo);

	int ret = pmalloc_operation(&pop->heap, 0, off, size, constructor, arg,
			extra_field, object_flags, class_id, &ctx);

	pmalloc_redo_release(pop);

	return ret;
}

/*
 * prealloc -- resizes in-place a previously allocated memory block
 *
 * The block offset is written persistently into the off variable.
 *
 * If successful function returns zero. Otherwise an error number is returned.
 */
int
prealloc(PMEMobjpool *pop, uint64_t *off, size_t size,
	uint64_t extra_field, uint16_t object_flags)
{
	struct redo_log *redo = pmalloc_redo_hold(pop);
	struct operation_context ctx;

	operation_init(&ctx, pop, pop->redo, redo);

	int ret = pmalloc_operation(&pop->heap, *off, off, size, NULL, NULL,
		extra_field, object_flags, 0, &ctx);

	pmalloc_redo_release(pop);

	return ret;
}

/*
 * pfree -- deallocates a memory block previously allocated by pmalloc
 *
 * A zero value is written persistently into the off variable.
 *
 * If successful function returns zero. Otherwise an error number is returned.
 */
void
pfree(PMEMobjpool *pop, uint64_t *off)
{
	struct redo_log *redo = pmalloc_redo_hold(pop);
	struct operation_context ctx;

	operation_init(&ctx, pop, pop->redo, redo);

	int ret = pmalloc_operation(&pop->heap, *off, off, 0, NULL, NULL,
		0, 0, 0, &ctx);
	ASSERTeq(ret, 0);

	pmalloc_redo_release(pop);
}

/*
 * pmalloc_construct_rt -- construct runtime part of allocator section
 */
static void *
pmalloc_construct_rt(PMEMobjpool *pop)
{
	return NULL;
}

/*
 * pmalloc_destroy_rt -- destroy runtime part of allocator section
 */
static void
pmalloc_destroy_rt(PMEMobjpool *pop, void *rt)
{
	/* NOP */
}

/*
 * pmalloc_recovery -- recovery of allocator lane section
 */
static int
pmalloc_recovery(PMEMobjpool *pop, void *data, unsigned length)
{
	struct lane_alloc_layout *sec = data;
	ASSERT(sizeof(*sec) <= length);

	redo_log_recover(pop->redo, sec->redo, ALLOC_REDO_LOG_SIZE);

	return 0;
}

/*
 * pmalloc_check -- consistency check of allocator lane section
 */
static int
pmalloc_check(PMEMobjpool *pop, void *data, unsigned length)
{
	LOG(3, "allocator lane %p", data);

	struct lane_alloc_layout *sec = data;

	int ret = redo_log_check(pop->redo, sec->redo, ALLOC_REDO_LOG_SIZE);
	if (ret != 0)
		ERR("allocator lane: redo log check failed");

	return ret;
}

/*
 * pmalloc_boot -- initializes allocator section
 */
static int
pmalloc_boot(PMEMobjpool *pop)
{
	int ret = palloc_boot(&pop->heap, (char *)pop + pop->heap_offset,
			pop->heap_size, pop->run_id, pop, &pop->p_ops);
	if (ret)
		return ret;

#ifdef USE_VG_MEMCHECK
	palloc_heap_vg_open(&pop->heap, pop->vg_boot);
#endif

	ret = palloc_buckets_init(&pop->heap);
	if (ret)
		palloc_heap_cleanup(&pop->heap);

	return ret;
}

static struct section_operations allocator_ops = {
	.construct_rt = pmalloc_construct_rt,
	.destroy_rt = pmalloc_destroy_rt,
	.recover = pmalloc_recovery,
	.check = pmalloc_check,
	.boot = pmalloc_boot
};

SECTION_PARM(LANE_SECTION_ALLOCATOR, &allocator_ops);

/*
 * CTL_WRITE_HANDLER(proto) -- creates a new allocation class
 */
static int
CTL_WRITE_HANDLER(desc)(PMEMobjpool *pop,
	enum ctl_query_type type, void *arg, struct ctl_indexes *indexes)
{
	uint8_t id;
	struct alloc_class_collection *ac = heap_alloc_classes(&pop->heap);

	if (SLIST_EMPTY(indexes)) {
		if (alloc_class_find_first_free_slot(ac, &id) != 0) {
			ERR("no available free allocation class identifier");
			errno = EINVAL;
			return -1;			
		}
	} else {
		struct ctl_index *idx = SLIST_FIRST(indexes);
		ASSERTeq(strcmp(idx->name, "class_id"), 0);

		if (idx->value < 0 || idx->value > MAX_ALLOCATION_CLASSES) {
			ERR("class id outside of the allowed range");
			errno = ERANGE;
			return -1;
		}

		id = (uint8_t)idx->value;

		if (alloc_class_by_id(ac, id) != NULL) {
			ERR("attempted to overwrite an allocation class");
			errno = EEXIST;
			return -1;
		}
	}

	struct pobj_alloc_class_desc *p = arg;
	p->class_id = id;

	struct alloc_class c;
	c.id = id;

	enum header_type lib_htype = MAX_HEADER_TYPES;
	switch (p->header_type) {
		case POBJ_HEADER_LEGACY:
			lib_htype = HEADER_LEGACY;
			break;
		case POBJ_HEADER_COMPACT:
			lib_htype = HEADER_COMPACT;
			break;
		case POBJ_HEADER_NONE:
			lib_htype = HEADER_NONE;
			break;
		default:
			ASSERT(0); /* unreachable */
			break;
	}

	c.header_type = lib_htype;
	c.type = CLASS_RUN;
	c.unit_size = p->unit_size;
	size_t runsize_bytes =
		CHUNKSIZE_ALIGN(p->units_per_block * p->unit_size);
	c.run.size_idx = (uint32_t)(runsize_bytes / CHUNKSIZE);

	alloc_class_generate_run_proto(&c.run, c.unit_size, c.run.size_idx);

	struct alloc_class *realc = alloc_class_register(
		heap_alloc_classes(&pop->heap), &c);

	if (heap_create_alloc_class_buckets(&pop->heap, realc) != 0) {
		alloc_class_delete(ac, realc);
		return -1;
	}

	return 0;
}

/*
 * pmalloc_header_type_parser -- parses the alloc header type argument
 */
static int
pmalloc_header_type_parser(const void *arg, void *dest, size_t dest_size)
{
	const char *vstr = arg;
	enum pobj_header_type *htype = dest;
	ASSERTeq(dest_size, sizeof(enum pobj_header_type));

	if (strcmp(vstr, "none") == 0) {
		*htype = POBJ_HEADER_NONE;
	} else if (strcmp(vstr, "compact") == 0) {
		*htype = POBJ_HEADER_COMPACT;
	} else if (strcmp(vstr, "legacy") == 0) {
		*htype = POBJ_HEADER_LEGACY;
	} else {
		ERR("invalid header type");
		errno = EINVAL;
		return -1;
	}

	return 0;
}

/*
 * CTL_READ_HANDLER(proto) -- reads the information about allocation class
 */
static int
CTL_READ_HANDLER(desc)(PMEMobjpool *pop,
	enum ctl_query_type type, void *arg, struct ctl_indexes *indexes)
{
	uint8_t id;

	struct ctl_index *idx = SLIST_FIRST(indexes);
	ASSERTeq(strcmp(idx->name, "class_id"), 0);

	if (idx->value < 0 || idx->value > MAX_ALLOCATION_CLASSES) {
		ERR("class id outside of the allowed range");
		errno = ERANGE;
		return -1;
	}

	id = (uint8_t)idx->value;

	struct alloc_class *c = alloc_class_by_id(
		heap_alloc_classes(&pop->heap), id);

	if (c == NULL) {
		ERR("class with the given id does not exist");
		errno = EEXIST;
		return -1;
	}

	ASSERTeq(c->type, CLASS_RUN);

	enum pobj_header_type user_htype = MAX_POBJ_HEADER_TYPES;
	switch (c->header_type) {
		case HEADER_LEGACY:
			user_htype = POBJ_HEADER_LEGACY;
			break;
		case HEADER_COMPACT:
			user_htype = POBJ_HEADER_COMPACT;
			break;
		case HEADER_NONE:
			user_htype = POBJ_HEADER_NONE;
			break;
		default:
			ASSERT(0); /* unreachable */
			break;
	}

	struct pobj_alloc_class_desc *p = arg;
	p->units_per_block = c->run.bitmap_nallocs;
	p->header_type = user_htype;
	p->unit_size = c->unit_size;
	p->class_id = c->id;

	return 0;
}

static struct ctl_argument CTL_ARG(desc) = {
	.dest_size = sizeof(struct pobj_alloc_class_desc),
	.parsers = {
		CTL_ARG_PARSER_STRUCT(struct pobj_alloc_class_desc,
			unit_size, ctl_arg_integer),
		CTL_ARG_PARSER_STRUCT(struct pobj_alloc_class_desc,
			units_per_block, ctl_arg_integer),
		CTL_ARG_PARSER_STRUCT(struct pobj_alloc_class_desc,
			header_type, pmalloc_header_type_parser),
		CTL_ARG_PARSER_END
	}
};

static const struct ctl_node CTL_NODE(class_id)[] = {
	CTL_LEAF_RW(desc),

	CTL_NODE_END
};

static const struct ctl_node CTL_NODE(new)[] = {
	CTL_LEAF_WO(desc),

	CTL_NODE_END
};

/*
 * CTL_WRITE_HANDLER(range) -- sets the map in range to the allocation class
 */
static int
CTL_WRITE_HANDLER(range)(PMEMobjpool *pop,
	enum ctl_query_type type, void *arg, struct ctl_indexes *indexes)
{
	struct pobj_alloc_class_map_range *range = arg;

	struct alloc_class *c = alloc_class_by_id(
		heap_alloc_classes(&pop->heap), range->class_id);

	if (c == NULL)
		return -1;

	int ret = alloc_class_range_set(heap_alloc_classes(&pop->heap),
		c, range->start, range->end);

	if (ret != 0) {
		ERR("range setting would be invalid for the class");
	}
	return ret;
}

static struct ctl_argument CTL_ARG(range) = {
	.dest_size = sizeof(struct pobj_alloc_class_map_range),
	.parsers = {
		CTL_ARG_PARSER_STRUCT(struct pobj_alloc_class_map_range,
			start, ctl_arg_integer),
		CTL_ARG_PARSER_STRUCT(struct pobj_alloc_class_map_range,
			end, ctl_arg_integer),
		CTL_ARG_PARSER_STRUCT(struct pobj_alloc_class_map_range,
			class_id, ctl_arg_integer),
		CTL_ARG_PARSER_END
	}
};

/*
 * CTL_WRITE_HANDLER(limit) -- reads the limit of allocation classes
 */
static int
CTL_READ_HANDLER(limit)(PMEMobjpool *pop,
	enum ctl_query_type type, void *arg, struct ctl_indexes *indexes)
{
	size_t *limit = arg;

	*limit = alloc_class_limit(heap_alloc_classes(&pop->heap));

	return 0;
}

/*
 * CTL_WRITE_HANDLER(granularity) -- reads the granularity of allocation classes
 */
static int
CTL_READ_HANDLER(granularity)(PMEMobjpool *pop,
	enum ctl_query_type type, void *arg, struct ctl_indexes *indexes)
{
	size_t *granularity = arg;

	*granularity = alloc_class_granularity(heap_alloc_classes(&pop->heap));

	return 0;
}

static const struct ctl_node CTL_NODE(map)[] = {
	CTL_LEAF_WO(range),
	CTL_LEAF_RO(limit),
	CTL_LEAF_RO(granularity),

	CTL_NODE_END
};

/*
 * CTL_WRITE_HANDLER(reset) -- resets the allocation classes
 */
static int
CTL_WRITE_HANDLER(reset)(PMEMobjpool *pop,
	enum ctl_query_type type, void *arg, struct ctl_indexes *indexes)
{
	struct pobj_alloc_class_params *params = arg;
	int ret = alloc_class_reset(heap_alloc_classes(&pop->heap),
		params->granularity, params->limit,
		params->fail_no_matching_class);

	if (ret != 0)
		return ret;

	heap_buckets_reset(&pop->heap);

	return 0;
}

static struct ctl_argument CTL_ARG(reset) = {
	.dest_size = sizeof(struct pobj_alloc_class_params),
	.parsers = {
		CTL_ARG_PARSER_STRUCT(struct pobj_alloc_class_params,
			limit, ctl_arg_integer),
		CTL_ARG_PARSER_STRUCT(struct pobj_alloc_class_params,
			granularity, ctl_arg_integer),
		CTL_ARG_PARSER_STRUCT(struct pobj_alloc_class_params,
			fail_no_matching_class, ctl_arg_integer),
		CTL_ARG_PARSER_END
	}
};

static const struct ctl_node CTL_NODE(alloc_class)[] = {
	CTL_LEAF_WO(reset),
	CTL_INDEXED(class_id),
	CTL_INDEXED(new),
	CTL_CHILD(map),

	CTL_NODE_END
};

static const struct ctl_node CTL_NODE(heap)[] = {
	CTL_CHILD(alloc_class),

	CTL_NODE_END
};

/*
 * pmalloc_ctl_register -- registers ctl nodes for "heap" module
 */
void
pmalloc_ctl_register(PMEMobjpool *pop)
{
	CTL_REGISTER_MODULE(pop->ctl, heap);
}

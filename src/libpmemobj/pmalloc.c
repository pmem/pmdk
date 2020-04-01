// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2020, Intel Corporation */

/*
 * pmalloc.c -- implementation of pmalloc POSIX-like API
 *
 * This is the front-end part of the persistent memory allocator. It uses both
 * transient and persistent representation of the heap to provide memory blocks
 * in a reasonable time and with an acceptable common-case fragmentation.
 */

#include <inttypes.h>
#include "valgrind_internal.h"
#include "heap.h"
#include "lane.h"
#include "memblock.h"
#include "memops.h"
#include "obj.h"
#include "out.h"
#include "palloc.h"
#include "pmalloc.h"
#include "alloc_class.h"
#include "set.h"
#include "mmap.h"

enum pmalloc_operation_type {
	OPERATION_INTERNAL, /* used only for single, one-off operations */
	OPERATION_EXTERNAL, /* used for everything else, incl. large redos */

	MAX_OPERATION_TYPE,
};

struct lane_alloc_runtime {
	struct operation_context *ctx[MAX_OPERATION_TYPE];
};

/*
 * pmalloc_operation_hold_type -- acquires allocator lane section and returns a
 *	pointer to its operation context
 */
static struct operation_context *
pmalloc_operation_hold_type(PMEMobjpool *pop, enum pmalloc_operation_type type,
	int start)
{
	struct lane *lane;
	lane_hold(pop, &lane);
	struct operation_context *ctx = type == OPERATION_INTERNAL ?
		lane->internal : lane->external;

	if (start)
		operation_start(ctx);

	return ctx;
}

/*
 * pmalloc_operation_hold_type -- acquires allocator lane section and returns a
 *	pointer to its operation context without starting
 */
struct operation_context *
pmalloc_operation_hold_no_start(PMEMobjpool *pop)
{
	return pmalloc_operation_hold_type(pop, OPERATION_EXTERNAL, 0);
}

/*
 * pmalloc_operation_hold -- acquires allocator lane section and returns a
 *	pointer to its redo log
 */
struct operation_context *
pmalloc_operation_hold(PMEMobjpool *pop)
{
	return pmalloc_operation_hold_type(pop, OPERATION_EXTERNAL, 1);
}

/*
 * pmalloc_operation_release -- releases allocator lane section
 */
void
pmalloc_operation_release(PMEMobjpool *pop)
{
	lane_release(pop);
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
	struct operation_context *ctx =
		pmalloc_operation_hold_type(pop, OPERATION_INTERNAL, 1);

	int ret = palloc_operation(&pop->heap, 0, off, size, NULL, NULL,
		extra_field, object_flags, 0, 0, ctx);

	pmalloc_operation_release(pop);

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
	struct operation_context *ctx =
		pmalloc_operation_hold_type(pop, OPERATION_INTERNAL, 1);

	int ret = palloc_operation(&pop->heap, 0, off, size, constructor, arg,
			extra_field, object_flags, class_id, 0, ctx);

	pmalloc_operation_release(pop);

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
	struct operation_context *ctx =
		pmalloc_operation_hold_type(pop, OPERATION_INTERNAL, 1);

	int ret = palloc_operation(&pop->heap, *off, off, size, NULL, NULL,
		extra_field, object_flags, 0, 0, ctx);

	pmalloc_operation_release(pop);

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
	struct operation_context *ctx =
		pmalloc_operation_hold_type(pop, OPERATION_INTERNAL, 1);

	int ret = palloc_operation(&pop->heap, *off, off, 0, NULL, NULL,
		0, 0, 0, 0, ctx);
	ASSERTeq(ret, 0);

	pmalloc_operation_release(pop);
}

/*
 * pmalloc_boot -- global runtime init routine of allocator section
 */
int
pmalloc_boot(PMEMobjpool *pop)
{
	int ret = palloc_boot(&pop->heap, (char *)pop + pop->heap_offset,
			pop->set->poolsize - pop->heap_offset, &pop->heap_size,
			pop, &pop->p_ops,
			pop->stats, pop->set);
	if (ret)
		return ret;

#if VG_MEMCHECK_ENABLED
	if (On_memcheck)
		palloc_heap_vg_open(&pop->heap, pop->vg_boot);
#endif

	ret = palloc_buckets_init(&pop->heap);
	if (ret)
		palloc_heap_cleanup(&pop->heap);

	return ret;
}

/*
 * pmalloc_cleanup -- global cleanup routine of allocator section
 */
int
pmalloc_cleanup(PMEMobjpool *pop)
{
	palloc_heap_cleanup(&pop->heap);

	return 0;
}

/*
 * CTL_WRITE_HANDLER(desc) -- creates a new allocation class
 */
static int
CTL_WRITE_HANDLER(desc)(void *ctx,
	enum ctl_query_source source, void *arg, struct ctl_indexes *indexes)
{
	PMEMobjpool *pop = ctx;
	uint8_t id;
	struct alloc_class_collection *ac = heap_alloc_classes(&pop->heap);
	struct pobj_alloc_class_desc *p = arg;

	if (p->unit_size <= 0 || p->unit_size > PMEMOBJ_MAX_ALLOC_SIZE ||
		p->units_per_block <= 0) {
		errno = EINVAL;
		return -1;
	}

	if (p->alignment != 0 && p->unit_size % p->alignment != 0) {
		ERR("unit size must be evenly divisible by alignment");
		errno = EINVAL;
		return -1;
	}

	if (p->alignment > (MEGABYTE * 2)) {
		ERR("alignment cannot be larger than 2 megabytes");
		errno = EINVAL;
		return -1;
	}

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
		case MAX_POBJ_HEADER_TYPES:
		default:
			ERR("invalid header type");
			errno = EINVAL;
			return -1;
	}

	if (PMDK_SLIST_EMPTY(indexes)) {
		if (alloc_class_find_first_free_slot(ac, &id) != 0) {
			ERR("no available free allocation class identifier");
			errno = EINVAL;
			return -1;
		}
	} else {
		struct ctl_index *idx = PMDK_SLIST_FIRST(indexes);
		ASSERTeq(strcmp(idx->name, "class_id"), 0);

		if (idx->value < 0 || idx->value >= MAX_ALLOCATION_CLASSES) {
			ERR("class id outside of the allowed range");
			errno = ERANGE;
			return -1;
		}

		id = (uint8_t)idx->value;

		if (alloc_class_reserve(ac, id) != 0) {
			ERR("attempted to overwrite an allocation class");
			errno = EEXIST;
			return -1;
		}
	}

	size_t runsize_bytes =
		CHUNK_ALIGN_UP((p->units_per_block * p->unit_size) +
		RUN_BASE_METADATA_SIZE);

	/* aligning the buffer might require up-to to 'alignment' bytes */
	if (p->alignment != 0)
		runsize_bytes += p->alignment;

	uint32_t size_idx = (uint32_t)(runsize_bytes / CHUNKSIZE);
	if (size_idx > UINT16_MAX)
		size_idx = UINT16_MAX;

	struct alloc_class *c = alloc_class_new(id,
		heap_alloc_classes(&pop->heap), CLASS_RUN,
		lib_htype, p->unit_size, p->alignment, size_idx);
	if (c == NULL) {
		errno = EINVAL;
		return -1;
	}

	if (heap_create_alloc_class_buckets(&pop->heap, c) != 0) {
		alloc_class_delete(ac, c);
		return -1;
	}

	p->class_id = c->id;
	p->units_per_block = c->run.nallocs;

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
 * CTL_READ_HANDLER(desc) -- reads the information about allocation class
 */
static int
CTL_READ_HANDLER(desc)(void *ctx,
	enum ctl_query_source source, void *arg, struct ctl_indexes *indexes)
{
	PMEMobjpool *pop = ctx;
	uint8_t id;

	struct ctl_index *idx = PMDK_SLIST_FIRST(indexes);
	ASSERTeq(strcmp(idx->name, "class_id"), 0);

	if (idx->value < 0 || idx->value >= MAX_ALLOCATION_CLASSES) {
		ERR("class id outside of the allowed range");
		errno = ERANGE;
		return -1;
	}

	id = (uint8_t)idx->value;

	struct alloc_class *c = alloc_class_by_id(
		heap_alloc_classes(&pop->heap), id);

	if (c == NULL) {
		ERR("class with the given id does not exist");
		errno = ENOENT;
		return -1;
	}

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
	p->units_per_block = c->type == CLASS_HUGE ? 0 : c->run.nallocs;
	p->header_type = user_htype;
	p->unit_size = c->unit_size;
	p->class_id = c->id;
	p->alignment = c->flags & CHUNK_FLAG_ALIGNED ? c->run.alignment : 0;

	return 0;
}

static const struct ctl_argument CTL_ARG(desc) = {
	.dest_size = sizeof(struct pobj_alloc_class_desc),
	.parsers = {
		CTL_ARG_PARSER_STRUCT(struct pobj_alloc_class_desc,
			unit_size, ctl_arg_integer),
		CTL_ARG_PARSER_STRUCT(struct pobj_alloc_class_desc,
			alignment, ctl_arg_integer),
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

static const struct ctl_node CTL_NODE(alloc_class)[] = {
	CTL_INDEXED(class_id),
	CTL_INDEXED(new),

	CTL_NODE_END
};

/*
 * CTL_RUNNABLE_HANDLER(extend) -- extends the pool by the given size
 */
static int
CTL_RUNNABLE_HANDLER(extend)(void *ctx,
	enum ctl_query_source source, void *arg, struct ctl_indexes *indexes)
{
	PMEMobjpool *pop = ctx;

	ssize_t arg_in = *(ssize_t *)arg;
	if (arg_in < (ssize_t)PMEMOBJ_MIN_PART) {
		ERR("incorrect size for extend, must be larger than %" PRIu64,
			PMEMOBJ_MIN_PART);
		return -1;
	}

	struct palloc_heap *heap = &pop->heap;
	struct bucket *defb = heap_bucket_acquire(heap,
		DEFAULT_ALLOC_CLASS_ID,
		HEAP_ARENA_PER_THREAD);

	int ret = heap_extend(heap, defb, (size_t)arg_in) < 0 ? -1 : 0;

	heap_bucket_release(heap, defb);

	return ret;
}

/*
 * CTL_READ_HANDLER(granularity) -- reads the current heap grow size
 */
static int
CTL_READ_HANDLER(granularity)(void *ctx,
	enum ctl_query_source source, void *arg, struct ctl_indexes *indexes)
{
	PMEMobjpool *pop = ctx;

	ssize_t *arg_out = arg;

	*arg_out = (ssize_t)pop->heap.growsize;

	return 0;
}

/*
 * CTL_WRITE_HANDLER(granularity) -- changes the heap grow size
 */
static int
CTL_WRITE_HANDLER(granularity)(void *ctx,
	enum ctl_query_source source, void *arg, struct ctl_indexes *indexes)
{
	PMEMobjpool *pop = ctx;

	ssize_t arg_in = *(int *)arg;
	if (arg_in != 0 && arg_in < (ssize_t)PMEMOBJ_MIN_PART) {
		ERR("incorrect grow size, must be 0 or larger than %" PRIu64,
			PMEMOBJ_MIN_PART);
		return -1;
	}

	pop->heap.growsize = (size_t)arg_in;

	return 0;
}

static const struct ctl_argument CTL_ARG(granularity) = CTL_ARG_LONG_LONG;

/*
 * CTL_READ_HANDLER(total) -- reads a number of the arenas
 */
static int
CTL_READ_HANDLER(total)(void *ctx,
	enum ctl_query_source source, void *arg, struct ctl_indexes *indexes)
{
	PMEMobjpool *pop = ctx;
	unsigned *narenas = arg;

	*narenas = heap_get_narenas_total(&pop->heap);

	return 0;
}

/*
 * CTL_READ_HANDLER(max) -- reads a max number of the arenas
 */
static int
CTL_READ_HANDLER(max)(void *ctx,
	enum ctl_query_source source, void *arg, struct ctl_indexes *indexes)
{
	PMEMobjpool *pop = ctx;
	unsigned *max = arg;

	*max = heap_get_narenas_max(&pop->heap);

	return 0;
}

/*
 * CTL_WRITE_HANDLER(max) -- write a max number of the arenas
 */
static int
CTL_WRITE_HANDLER(max)(void *ctx,
	enum ctl_query_source source, void *arg, struct ctl_indexes *indexes)
{
	PMEMobjpool *pop = ctx;
	unsigned size = *(unsigned *)arg;

	int ret = heap_set_narenas_max(&pop->heap, size);
	if (ret) {
		LOG(1, "cannot change max arena number");
		return -1;
	}

	return 0;
}

static const struct ctl_argument CTL_ARG(max) = CTL_ARG_LONG_LONG;

/*
 * CTL_READ_HANDLER(automatic) -- reads a number of the automatic arenas
 */
static int
CTL_READ_HANDLER(automatic, narenas)(void *ctx,
	enum ctl_query_source source, void *arg, struct ctl_indexes *indexes)
{
	PMEMobjpool *pop = ctx;
	unsigned *narenas = arg;

	*narenas = heap_get_narenas_auto(&pop->heap);

	return 0;
}

/*
 * CTL_READ_HANDLER(arena_id) -- reads the id of the arena
 * assigned to the calling thread
 */
static int
CTL_READ_HANDLER(arena_id)(void *ctx,
	enum ctl_query_source source, void *arg, struct ctl_indexes *indexes)
{
	PMEMobjpool *pop = ctx;
	unsigned *arena_id = arg;

	*arena_id = heap_get_thread_arena_id(&pop->heap);

	return 0;
}

/*
 * CTL_WRITE_HANDLER(arena_id) -- assigns the arena to the calling thread
 */
static int
CTL_WRITE_HANDLER(arena_id)(void *ctx,
	enum ctl_query_source source, void *arg, struct ctl_indexes *indexes)
{
	PMEMobjpool *pop = ctx;
	unsigned arena_id = *(unsigned *)arg;

	unsigned narenas = heap_get_narenas_total(&pop->heap);

	/*
	 * check if index is not bigger than number of arenas
	 * or if it is not equal zero
	 */
	if (arena_id < 1 || arena_id > narenas) {
		LOG(1, "arena id outside of the allowed range: <1,%u>",
			narenas);
		errno = ERANGE;
		return -1;
	}

	heap_set_arena_thread(&pop->heap, arena_id);

	return 0;
}

static const struct ctl_argument CTL_ARG(arena_id) = CTL_ARG_LONG_LONG;

/*
 * CTL_WRITE_HANDLER(automatic) -- updates automatic status of the arena
 */
static int
CTL_WRITE_HANDLER(automatic)(void *ctx, enum ctl_query_source source,
		void *arg, struct ctl_indexes *indexes)
{
	PMEMobjpool *pop = ctx;
	int arg_in = *(int *)arg;
	unsigned arena_id;

	struct ctl_index *idx = PMDK_SLIST_FIRST(indexes);
	ASSERTeq(strcmp(idx->name, "arena_id"), 0);
	arena_id = (unsigned)idx->value;

	unsigned narenas = heap_get_narenas_total(&pop->heap);

	/*
	 * check if index is not bigger than number of arenas
	 * or if it is not equal zero
	 */
	if (arena_id < 1 || arena_id > narenas) {
		LOG(1, "arena id outside of the allowed range: <1,%u>",
			narenas);
		errno = ERANGE;
		return -1;
	}

	if (arg_in != 0 && arg_in != 1) {
		LOG(1, "incorrect arena state, must be 0 or 1");
		return -1;
	}

	return heap_set_arena_auto(&pop->heap, arena_id, arg_in);
}

/*
 * CTL_READ_HANDLER(automatic) -- reads automatic status of the arena
 */
static int
CTL_READ_HANDLER(automatic)(void *ctx,
	enum ctl_query_source source, void *arg, struct ctl_indexes *indexes)
{
	PMEMobjpool *pop = ctx;
	int *arg_out = arg;
	unsigned arena_id;

	struct ctl_index *idx = PMDK_SLIST_FIRST(indexes);
	ASSERTeq(strcmp(idx->name, "arena_id"), 0);
	arena_id = (unsigned)idx->value;

	unsigned narenas = heap_get_narenas_total(&pop->heap);

	/*
	 * check if index is not bigger than number of arenas
	 * or if it is not equal zero
	 */
	if (arena_id < 1 || arena_id > narenas) {
		LOG(1, "arena id outside of the allowed range: <1,%u>",
			narenas);
		errno = ERANGE;
		return -1;
	}

	*arg_out = heap_get_arena_auto(&pop->heap, arena_id);

	return 0;
}

static struct ctl_argument CTL_ARG(automatic) = CTL_ARG_BOOLEAN;

static const struct ctl_node CTL_NODE(size)[] = {
	CTL_LEAF_RW(granularity),
	CTL_LEAF_RUNNABLE(extend),

	CTL_NODE_END
};

/*
 * CTL_READ_HANDLER(size) -- reads usable size of specified arena
 */
static int
CTL_READ_HANDLER(size)(void *ctx,
	enum ctl_query_source source, void *arg, struct ctl_indexes *indexes)
{
	PMEMobjpool *pop = ctx;
	unsigned arena_id;
	unsigned narenas;
	size_t *arena_size = arg;

	struct ctl_index *idx = PMDK_SLIST_FIRST(indexes);
	ASSERTeq(strcmp(idx->name, "arena_id"), 0);

	/* take index of arena */
	arena_id = (unsigned)idx->value;
	/* take number of arenas */
	narenas = heap_get_narenas_total(&pop->heap);

	/*
	 * check if index is not bigger than number of arenas
	 * or if it is not equal zero
	 */
	if (arena_id < 1 || arena_id > narenas) {
		LOG(1, "arena id outside of the allowed range: <1,%u>",
			narenas);
		errno = ERANGE;
		return -1;
	}

	/* take buckets for arena */
	struct bucket **buckets;
	buckets = heap_get_arena_buckets(&pop->heap, arena_id);

	/* calculate number of reservation for arena using buckets */
	unsigned size = 0;
	for (int i = 0; i < MAX_ALLOCATION_CLASSES; ++i) {
		if (buckets[i] != NULL && buckets[i]->is_active)
			size += buckets[i]->active_memory_block->m.size_idx;
	}

	*arena_size = size * CHUNKSIZE;

	return 0;
}

/*
 * CTL_RUNNABLE_HANDLER(create) -- create new arena in the heap
 */
static int
CTL_RUNNABLE_HANDLER(create)(void *ctx,
	enum ctl_query_source source, void *arg, struct ctl_indexes *indexes)
{
	PMEMobjpool *pop = ctx;
	unsigned *arena_id = arg;
	struct palloc_heap *heap = &pop->heap;

	int ret = heap_arena_create(heap);
	if (ret < 0)
		return -1;

	*arena_id = (unsigned)ret;

	return 0;
}

static const struct ctl_node CTL_NODE(arena_id)[] = {
	CTL_LEAF_RO(size),
	CTL_LEAF_RW(automatic),

	CTL_NODE_END
};

static const struct ctl_node CTL_NODE(arena)[] = {
	CTL_INDEXED(arena_id),
	CTL_LEAF_RUNNABLE(create),

	CTL_NODE_END
};

static const struct ctl_node CTL_NODE(narenas)[] = {
	CTL_LEAF_RO(automatic, narenas),
	CTL_LEAF_RO(total),
	CTL_LEAF_RW(max),

	CTL_NODE_END
};

static const struct ctl_node CTL_NODE(thread)[] = {
	CTL_LEAF_RW(arena_id),

	CTL_NODE_END
};

static const struct ctl_node CTL_NODE(heap)[] = {
	CTL_CHILD(alloc_class),
	CTL_CHILD(arena),
	CTL_CHILD(size),
	CTL_CHILD(thread),
	CTL_CHILD(narenas),

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

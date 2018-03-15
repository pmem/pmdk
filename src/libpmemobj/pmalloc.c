/*
 * Copyright 2015-2018, Intel Corporation
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

#include <inttypes.h>
#include "valgrind_internal.h"
#include "heap.h"
#include "lane.h"
#include "memops.h"
#include "obj.h"
#include "out.h"
#include "palloc.h"
#include "pmalloc.h"
#include "alloc_class.h"
#include "set.h"
#include "mmap.h"

#define PMALLOC_REDO_LOG_EXTEND_SIZE 251 /* 4096 - 16 */

enum pmalloc_operation_type {
	OPERATION_INTERNAL,
	OPERATION_EXTERNAL,

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
	struct lane_section *lane;
	lane_hold(pop, &lane, LANE_SECTION_ALLOCATOR);
	struct lane_alloc_runtime *rt = lane->runtime;

	if (start)
		operation_start(rt->ctx[type]);
	else
		operation_init(rt->ctx[type]);

	return rt->ctx[type];
}

/*
 * pmalloc_operation_hold_type -- acquires allocator lane section and returns a
 *	pointer to it's operation context without starting
 */
struct operation_context *
pmalloc_operation_hold_no_start(PMEMobjpool *pop)
{
	return pmalloc_operation_hold_type(pop, OPERATION_EXTERNAL, 0);
}

/*
 * pmalloc_operation_hold -- acquires allocator lane section and returns a
 *	pointer to it's redo log
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
#if VG_MEMCHECK_ENABLED
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
	struct operation_context *ctx =
		pmalloc_operation_hold_type(pop, OPERATION_INTERNAL, 1);

	int ret = pmalloc_operation(&pop->heap, 0, off, size, NULL, NULL,
		extra_field, object_flags, 0, ctx);

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

	int ret = pmalloc_operation(&pop->heap, 0, off, size, constructor, arg,
			extra_field, object_flags, class_id, ctx);

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

	int ret = pmalloc_operation(&pop->heap, *off, off, size, NULL, NULL,
		extra_field, object_flags, 0, ctx);

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

	int ret = pmalloc_operation(&pop->heap, *off, off, 0, NULL, NULL,
		0, 0, 0, ctx);
	ASSERTeq(ret, 0);

	pmalloc_operation_release(pop);
}

/*
 * alloc_redo_constructor -- constructor of a new redo log section
 */
static int
alloc_redo_constructor(void *base, void *ptr, size_t usable_size, void *arg)
{
	PMEMobjpool *pop = base;
	const struct pmem_ops *p_ops = &pop->p_ops;
	VALGRIND_ADD_TO_TX(ptr, usable_size);

	struct redo_log *redo = ptr;
	redo->capacity = PMALLOC_REDO_LOG_EXTEND_SIZE;
	redo->checksum = 0;
	redo->next = 0;
	memset(redo->unused, 0, sizeof(redo->unused));

	pmemops_flush(p_ops, redo, sizeof(*redo));

	pmemops_memset(p_ops, redo->entries, 0,
		usable_size - sizeof(*redo), 0);

	VALGRIND_REMOVE_FROM_TX(ptr, usable_size);

	return 0;
}

/*
 * alloc_redo_external_extend -- redo log extend callback
 */
static int
alloc_redo_external_extend(void *base, uint64_t *redo)
{
	size_t s = SIZEOF_REDO_LOG(PMALLOC_REDO_LOG_EXTEND_SIZE);

	return pmalloc_construct(base, redo, s, alloc_redo_constructor, NULL, 0,
		OBJ_INTERNAL_OBJECT_MASK, 0);
}

/*
 * pmalloc_construct_rt -- construct runtime part of allocator section
 */
static void *
pmalloc_construct_rt(PMEMobjpool *pop, void *data)
{
	struct lane_alloc_layout *layout = data;
	struct lane_alloc_runtime *alloc_rt = Malloc(sizeof(*alloc_rt));
	if (alloc_rt == NULL)
		goto error_rt_alloc;

	alloc_rt->ctx[OPERATION_INTERNAL] = operation_new(pop, pop->redo,
		(struct redo_log *)&layout->internal, ALLOC_REDO_INTERNAL_SIZE,
		NULL);
	if (alloc_rt->ctx[OPERATION_INTERNAL] == NULL)
		goto error_internal_alloc;

	alloc_rt->ctx[OPERATION_EXTERNAL] = operation_new(pop, pop->redo,
		(struct redo_log *)&layout->external, ALLOC_REDO_EXTERNAL_SIZE,
		alloc_redo_external_extend);
	if (alloc_rt->ctx[OPERATION_EXTERNAL] == NULL)
		goto error_external_alloc;

	return alloc_rt;
error_external_alloc:
	Free(alloc_rt->ctx[OPERATION_INTERNAL]);
error_internal_alloc:
	Free(alloc_rt);
error_rt_alloc:
	return NULL;
}

/*
 * pmalloc_destroy_rt -- destroy runtime part of allocator section
 */
static void
pmalloc_destroy_rt(PMEMobjpool *pop, void *rt)
{
	struct lane_alloc_runtime *alloc_rt = rt;
	operation_delete(alloc_rt->ctx[OPERATION_INTERNAL]);
	operation_delete(alloc_rt->ctx[OPERATION_EXTERNAL]);
	Free(alloc_rt);
}

/*
 * pmalloc_recovery -- recovery of allocator lane section
 */
static int
pmalloc_recovery(PMEMobjpool *pop, void *data, unsigned length)
{
	struct lane_alloc_layout *sec = data;
	ASSERT(sizeof(*sec) <= length);

	redo_log_recover(pop->redo, (struct redo_log *)&sec->internal);
	redo_log_recover(pop->redo, (struct redo_log *)&sec->external);

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

	int ret = redo_log_check(pop->redo, (struct redo_log *)&sec->internal);
	if (ret != 0)
		ERR("allocator lane: internal redo log check failed");

	ret = redo_log_check(pop->redo, (struct redo_log *)&sec->external);
	if (ret != 0)
		ERR("allocator lane: external redo log check failed");


	return ret;
}

/*
 * pmalloc_boot -- global runtime init routine of allocator section
 */
static int
pmalloc_boot(PMEMobjpool *pop)
{
	int ret = palloc_boot(&pop->heap, (char *)pop + pop->heap_offset,
			pop->set->poolsize - pop->heap_offset, &pop->heap_size,
			pop, &pop->p_ops,
			pop->stats, pop->set);
	if (ret)
		return ret;

#if VG_MEMCHECK_ENABLED
	if (On_valgrind)
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
static int
pmalloc_cleanup(PMEMobjpool *pop)
{
	palloc_heap_cleanup(&pop->heap);

	return 0;
}

static struct section_operations allocator_ops = {
	.construct_rt = pmalloc_construct_rt,
	.destroy_rt = pmalloc_destroy_rt,
	.recover = pmalloc_recovery,
	.check = pmalloc_check,
	.boot = pmalloc_boot,
	.cleanup = pmalloc_cleanup,
};

SECTION_PARM(LANE_SECTION_ALLOCATOR, &allocator_ops);

/*
 * CTL_WRITE_HANDLER(proto) -- creates a new allocation class
 */
static int
CTL_WRITE_HANDLER(desc)(PMEMobjpool *pop,
	enum ctl_query_source source, void *arg, struct ctl_indexes *indexes)
{
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

	if (SLIST_EMPTY(indexes)) {
		if (alloc_class_find_first_free_slot(ac, &id) != 0) {
			ERR("no available free allocation class identifier");
			errno = EINVAL;
			return -1;
		}
	} else {
		struct ctl_index *idx = SLIST_FIRST(indexes);
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
		RUN_METASIZE);

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
CTL_READ_HANDLER(desc)(PMEMobjpool *pop,
	enum ctl_query_source source, void *arg, struct ctl_indexes *indexes)
{
	uint8_t id;

	struct ctl_index *idx = SLIST_FIRST(indexes);
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
	p->units_per_block = c->type == CLASS_HUGE ? 0 : c->run.bitmap_nallocs;
	p->header_type = user_htype;
	p->unit_size = c->unit_size;
	p->class_id = c->id;
	p->alignment = c->flags & CHUNK_FLAG_ALIGNED ? c->run.alignment : 0;

	return 0;
}

static struct ctl_argument CTL_ARG(desc) = {
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
CTL_RUNNABLE_HANDLER(extend)(PMEMobjpool *pop,
	enum ctl_query_source source, void *arg, struct ctl_indexes *indexes)
{
	ssize_t arg_in = *(ssize_t *)arg;
	if (arg_in < (ssize_t)PMEMOBJ_MIN_PART) {
		ERR("incorrect size for extend, must be larger than %" PRIu64,
			PMEMOBJ_MIN_PART);
		return -1;
	}

	struct palloc_heap *heap = &pop->heap;
	struct bucket *defb = heap_bucket_acquire_by_id(heap,
		DEFAULT_ALLOC_CLASS_ID);

	int ret = heap_extend(heap, defb, (size_t)arg_in) < 0 ? -1 : 0;

	heap_bucket_release(heap, defb);

	return ret;
}

/*
 * CTL_READ_HANDLER(granularity) -- reads the current heap grow size
 */
static int
CTL_READ_HANDLER(granularity)(PMEMobjpool *pop,
	enum ctl_query_source source, void *arg, struct ctl_indexes *indexes)
{
	ssize_t *arg_out = arg;

	*arg_out = (ssize_t)pop->heap.growsize;

	return 0;
}

/*
 * CTL_WRITE_HANDLER(granularity) -- changes the heap grow size
 */
static int
CTL_WRITE_HANDLER(granularity)(PMEMobjpool *pop,
	enum ctl_query_source source, void *arg, struct ctl_indexes *indexes)
{
	ssize_t arg_in = *(int *)arg;
	if (arg_in != 0 && arg_in < (ssize_t)PMEMOBJ_MIN_PART) {
		ERR("incorrect grow size, must be 0 or larger than %" PRIu64,
			PMEMOBJ_MIN_PART);
		return -1;
	}

	pop->heap.growsize = (size_t)arg_in;

	return 0;
}

static struct ctl_argument CTL_ARG(granularity) = CTL_ARG_LONG_LONG;

static const struct ctl_node CTL_NODE(size)[] = {
	CTL_LEAF_RW(granularity),
	CTL_LEAF_RUNNABLE(extend),

	CTL_NODE_END
};

static const struct ctl_node CTL_NODE(heap)[] = {
	CTL_CHILD(alloc_class),
	CTL_CHILD(size),

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

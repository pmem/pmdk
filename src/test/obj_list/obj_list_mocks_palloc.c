// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2019, Intel Corporation */

/*
 * obj_list_mocks_palloc.c -- mocks for palloc/pmalloc modules
 */

#include "obj_list.h"

/*
 * pmalloc -- pmalloc mock
 *
 * Allocates the memory using linear allocator.
 * Prints the id of allocated struct oob_item for tracking purposes.
 */
FUNC_MOCK(pmalloc, int, PMEMobjpool *pop, uint64_t *ptr,
	size_t size, uint64_t extra_field, uint16_t flags)
	FUNC_MOCK_RUN_DEFAULT {
		struct pmem_ops *p_ops = &Pop->p_ops;
		size = size + OOB_OFF + sizeof(uint64_t) * 2;
		uint64_t *alloc_size = (uint64_t *)((uintptr_t)Pop
				+ *Heap_offset);
		*alloc_size = size;
		pmemops_persist(p_ops, alloc_size, sizeof(*alloc_size));

		*ptr = *Heap_offset + sizeof(uint64_t);
		if (OBJ_PTR_FROM_POOL(pop, ptr))
			pmemops_persist(p_ops, ptr, sizeof(*ptr));

		struct oob_item *item =
			(struct oob_item *)((uintptr_t)Pop + *ptr);

		*ptr += OOB_OFF;
		if (OBJ_PTR_FROM_POOL(pop, ptr))
			pmemops_persist(p_ops, ptr, sizeof(*ptr));

		item->item.id = *Id;
		pmemops_persist(p_ops, &item->item.id, sizeof(item->item.id));

		(*Id)++;
		pmemops_persist(p_ops, Id, sizeof(*Id));

		*Heap_offset = *Heap_offset + sizeof(uint64_t) +
			size + OOB_OFF;
		pmemops_persist(p_ops, Heap_offset, sizeof(*Heap_offset));

		UT_OUT("pmalloc(id = %d)", item->item.id);
		return 0;
	}
FUNC_MOCK_END

/*
 * pfree -- pfree mock
 *
 * Just prints freeing struct oob_item id. Doesn't free the memory.
 */
FUNC_MOCK(pfree, void, PMEMobjpool *pop, uint64_t *ptr)
	FUNC_MOCK_RUN_DEFAULT {
		struct oob_item *item =
			(struct oob_item *)((uintptr_t)Pop + *ptr - OOB_OFF);
		UT_OUT("pfree(id = %d)", item->item.id);
		*ptr = 0;
		if (OBJ_PTR_FROM_POOL(pop, ptr))
			pmemops_persist(&Pop->p_ops, ptr, sizeof(*ptr));

		return;
	}
FUNC_MOCK_END

/*
 * pmalloc_construct -- pmalloc_construct mock
 *
 * Allocates the memory using linear allocator and invokes the constructor.
 * Prints the id of allocated struct oob_item for tracking purposes.
 */
FUNC_MOCK(pmalloc_construct, int, PMEMobjpool *pop, uint64_t *off,
	size_t size, palloc_constr constructor, void *arg,
	uint64_t extra_field, uint16_t flags, uint16_t class_id)
	FUNC_MOCK_RUN_DEFAULT {
		struct pmem_ops *p_ops = &Pop->p_ops;
		size = size + OOB_OFF + sizeof(uint64_t) * 2;
		uint64_t *alloc_size = (uint64_t *)((uintptr_t)Pop +
				*Heap_offset);
		*alloc_size = size;
		pmemops_persist(p_ops, alloc_size, sizeof(*alloc_size));

		*off = *Heap_offset + sizeof(uint64_t) + OOB_OFF;
		if (OBJ_PTR_FROM_POOL(pop, off))
			pmemops_persist(p_ops, off, sizeof(*off));

		*Heap_offset = *Heap_offset + sizeof(uint64_t) + size;
		pmemops_persist(p_ops, Heap_offset, sizeof(*Heap_offset));

		void *ptr = (void *)((uintptr_t)Pop + *off);
		constructor(pop, ptr, size, arg);

		return 0;
	}
FUNC_MOCK_END

/*
 * prealloc -- prealloc mock
 */
FUNC_MOCK(prealloc, int, PMEMobjpool *pop, uint64_t *off, size_t size,
	uint64_t extra_field, uint16_t flags)
	FUNC_MOCK_RUN_DEFAULT {
		uint64_t *alloc_size = (uint64_t *)((uintptr_t)Pop +
				*off - sizeof(uint64_t));
		struct item *item = (struct item *)((uintptr_t)Pop +
				*off + OOB_OFF);
		if (*alloc_size >= size) {
			*alloc_size = size;
			pmemops_persist(&Pop->p_ops, alloc_size,
					sizeof(*alloc_size));

			UT_OUT("prealloc(id = %d, size = %zu) = true",
				item->id,
				(size - OOB_OFF) / sizeof(struct item));
			return 0;
		} else {
			UT_OUT("prealloc(id = %d, size = %zu) = false",
				item->id,
				(size - OOB_OFF) / sizeof(struct item));
			return -1;
		}
	}
FUNC_MOCK_END

/*
 * prealloc_construct -- prealloc_construct mock
 */
FUNC_MOCK(prealloc_construct, int, PMEMobjpool *pop, uint64_t *off,
	size_t size, palloc_constr constructor, void *arg,
	uint64_t extra_field, uint16_t flags, uint16_t class_id)
	FUNC_MOCK_RUN_DEFAULT {
		int ret = __wrap_prealloc(pop, off, size, 0, 0);
		if (!ret) {
			void *ptr = (void *)((uintptr_t)Pop + *off + OOB_OFF);
			constructor(pop, ptr, size, arg);
		}
		return ret;
	}
FUNC_MOCK_END

/*
 * palloc_reserve -- palloc_reserve mock
 */
FUNC_MOCK(palloc_reserve, int, struct palloc_heap *heap, size_t size,
	palloc_constr constructor, void *arg,
	uint64_t extra_field, uint16_t object_flags, uint16_t class_id,
	uint16_t arena_id, struct pobj_action *act)
	FUNC_MOCK_RUN_DEFAULT {
		struct pmem_ops *p_ops = &Pop->p_ops;
		size = size + OOB_OFF + sizeof(uint64_t) * 2;
		uint64_t *alloc_size = (uint64_t *)((uintptr_t)Pop
			+ *Heap_offset);
		*alloc_size = size;
		pmemops_persist(p_ops, alloc_size, sizeof(*alloc_size));

		act->heap.offset = *Heap_offset + sizeof(uint64_t);

		struct oob_item *item =
			(struct oob_item *)((uintptr_t)Pop + act->heap.offset);

		act->heap.offset += OOB_OFF;
		item->item.id = *Id;
		pmemops_persist(p_ops, &item->item.id, sizeof(item->item.id));

		(*Id)++;
		pmemops_persist(p_ops, Id, sizeof(*Id));

		*Heap_offset += sizeof(uint64_t) + size + OOB_OFF;
		pmemops_persist(p_ops, Heap_offset, sizeof(*Heap_offset));

		UT_OUT("pmalloc(id = %d)", item->item.id);
		return 0;
	}
FUNC_MOCK_END

/*
 * palloc_publish -- mock publish, must process operation
 */
FUNC_MOCK(palloc_publish, void, struct palloc_heap *heap,
	struct pobj_action *actv, size_t actvcnt,
	struct operation_context *ctx)
	FUNC_MOCK_RUN_DEFAULT {
		operation_process(ctx);
		operation_finish(ctx, 0);
	}
FUNC_MOCK_END

/*
 * palloc_defer_free -- pfree mock
 *
 * Just prints freeing struct oob_item id. Doesn't free the memory.
 */
FUNC_MOCK(palloc_defer_free, void, struct palloc_heap *heap, uint64_t off,
	struct pobj_action *act)
	FUNC_MOCK_RUN_DEFAULT {
		struct oob_item *item =
			(struct oob_item *)((uintptr_t)Pop + off - OOB_OFF);
		UT_OUT("pfree(id = %d)", item->item.id);
		act->heap.offset = off;
		return;
	}
FUNC_MOCK_END

/*
 * pmalloc_usable_size -- pmalloc_usable_size mock
 */
FUNC_MOCK(palloc_usable_size, size_t, struct palloc_heap *heap, uint64_t off)
	FUNC_MOCK_RUN_DEFAULT {
		uint64_t *alloc_size = (uint64_t *)((uintptr_t)Pop +
				off - sizeof(uint64_t));
		return (size_t)*alloc_size;
	}
FUNC_MOCK_END

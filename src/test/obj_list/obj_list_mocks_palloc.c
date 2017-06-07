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
		pmemops_persist(p_ops, ptr, sizeof(*ptr));

		struct oob_item *item =
			(struct oob_item *)((uintptr_t)Pop + *ptr);

		*ptr += OOB_OFF;
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
 * pmalloc_usable_size -- pmalloc_usable_size mock
 */
FUNC_MOCK(palloc_usable_size, size_t, struct palloc_heap *heap, uint64_t off)
	FUNC_MOCK_RUN_DEFAULT {
		uint64_t *alloc_size = (uint64_t *)((uintptr_t)Pop +
				off - sizeof(uint64_t));
		return (size_t)*alloc_size;
	}
FUNC_MOCK_END

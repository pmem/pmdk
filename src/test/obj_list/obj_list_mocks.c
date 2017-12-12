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
 * obj_list_mocks.c -- mocks for redo/lane/heap/obj modules
 */

#include "valgrind_internal.h"
#include "obj_list.h"

/*
 * pmem_drain_nop -- no operation for drain on non-pmem memory
 */
static void
pmem_drain_nop(void)
{
	/* NOP */
}

/*
 * obj_persist -- pmemobj version of pmem_persist w/o replication
 */
static void
obj_persist(void *ctx, const void *addr, size_t len)
{
	PMEMobjpool *pop = (PMEMobjpool *)ctx;
	pop->persist_local(addr, len);
}

/*
 * obj_flush -- pmemobj version of pmem_flush w/o replication
 */
static void
obj_flush(void *ctx, const void *addr, size_t len)
{
	PMEMobjpool *pop = (PMEMobjpool *)ctx;
	pop->flush_local(addr, len);
}

/*
 * obj_drain -- pmemobj version of pmem_drain w/o replication
 */
static void
obj_drain(void *ctx)
{
	PMEMobjpool *pop = (PMEMobjpool *)ctx;
	pop->drain_local();
}

/*
 * linear_alloc -- allocates `size` bytes (rounded up to 8 bytes) and returns
 * offset to the allocated object
 */
static uint64_t
linear_alloc(uint64_t *cur_offset, size_t size)
{
	uint64_t ret = *cur_offset;
	*cur_offset += roundup(size, sizeof(uint64_t));
	return ret;
}

/*
 * redo_log_check_offset -- (internal) check if offset is valid
 *
 * XXX: copy & paste from obj.c (since it's static)
 */
static int
redo_log_check_offset(void *ctx, uint64_t offset)
{
	PMEMobjpool *pop = (PMEMobjpool *)ctx;
	return OBJ_OFF_IS_VALID(pop, offset);
}

/*
 * pmemobj_open -- pmemobj_open mock
 *
 * This function initializes the pmemobj pool for purposes of this
 * unittest.
 */
FUNC_MOCK(pmemobj_open, PMEMobjpool *, const char *fname, const char *layout)
FUNC_MOCK_RUN_DEFAULT
{
	size_t size;
	int is_pmem;

	void *addr = pmem_map_file(fname, 0, 0, 0, &size, &is_pmem);
	if (!addr) {
		UT_OUT("!%s: pmem_map_file", fname);
		return NULL;
	}

	Pop = (PMEMobjpool *)addr;
	Pop->addr = Pop;
	Pop->size = size;
	Pop->is_pmem = is_pmem;
	Pop->rdonly = 0;
	Pop->uuid_lo = 0x12345678;

	VALGRIND_REMOVE_PMEM_MAPPING(&Pop->mutex_head,
		sizeof(Pop->mutex_head));
	VALGRIND_REMOVE_PMEM_MAPPING(&Pop->rwlock_head,
		sizeof(Pop->rwlock_head));
	VALGRIND_REMOVE_PMEM_MAPPING(&Pop->cond_head,
		sizeof(Pop->cond_head));
	Pop->mutex_head = NULL;
	Pop->rwlock_head = NULL;
	Pop->cond_head = NULL;

	if (Pop->is_pmem) {
		Pop->persist_local = pmem_persist;
		Pop->flush_local = pmem_flush;
		Pop->drain_local = pmem_drain;
	} else {
		Pop->persist_local = (persist_local_fn)pmem_msync;
		Pop->flush_local = (persist_local_fn)pmem_msync;
		Pop->drain_local = pmem_drain_nop;
	}

	Pop->p_ops.persist = obj_persist;
	Pop->p_ops.flush = obj_flush;
	Pop->p_ops.drain = obj_drain;
	Pop->p_ops.base = Pop;
	struct pmem_ops *p_ops = &Pop->p_ops;

	Pop->heap_offset = HEAP_OFFSET;
	Pop->heap_size = Pop->size - Pop->heap_offset;
	uint64_t heap_offset = HEAP_OFFSET;

	Heap_offset = (uint64_t *)((uintptr_t)Pop +
			linear_alloc(&heap_offset, sizeof(*Heap_offset)));

	Id = (int *)((uintptr_t)Pop + linear_alloc(&heap_offset, sizeof(*Id)));

	/* Alloc lane layout */
	Lane_section.layout = (struct lane_section_layout *)((uintptr_t)Pop +
			linear_alloc(&heap_offset, LANE_SECTION_LEN));

	/* Alloc in band lists */
	List.oid.pool_uuid_lo = Pop->uuid_lo;
	List.oid.off = linear_alloc(&heap_offset, sizeof(struct list));

	List_sec.oid.pool_uuid_lo = Pop->uuid_lo;
	List_sec.oid.off = linear_alloc(&heap_offset, sizeof(struct list));

	/* Alloc out of band lists */
	List_oob.oid.pool_uuid_lo = Pop->uuid_lo;
	List_oob.oid.off = linear_alloc(&heap_offset, sizeof(struct oob_list));

	List_oob_sec.oid.pool_uuid_lo = Pop->uuid_lo;
	List_oob_sec.oid.off =
			linear_alloc(&heap_offset, sizeof(struct oob_list));

	Item = (union oob_item_toid *)((uintptr_t)Pop +
			linear_alloc(&heap_offset, sizeof(*Item)));
	Item->oid.pool_uuid_lo = Pop->uuid_lo;
	Item->oid.off = linear_alloc(&heap_offset, sizeof(struct oob_item));
	pmemops_persist(p_ops, Item, sizeof(*Item));

	if (*Heap_offset == 0) {
		*Heap_offset = heap_offset;
		pmemops_persist(p_ops, Heap_offset, sizeof(*Heap_offset));
	}

	pmemops_persist(p_ops, Pop, HEAP_OFFSET);

	Pop->run_id += 2;
	pmemops_persist(p_ops, &Pop->run_id, sizeof(Pop->run_id));

	Pop->redo = redo_log_config_new(Pop->addr, p_ops, redo_log_check_offset,
			Pop, REDO_NUM_ENTRIES);
	pmemops_persist(p_ops, &Pop->redo, sizeof(Pop->redo));

	return Pop;
}
FUNC_MOCK_END

/*
 * pmemobj_close -- pmemobj_close mock
 *
 * Just unmap the mapped area.
 */
FUNC_MOCK(pmemobj_close, void, PMEMobjpool *pop)
	FUNC_MOCK_RUN_DEFAULT {
		redo_log_config_delete(Pop->redo);
		UT_ASSERTeq(pmem_unmap(Pop, Pop->size), 0);
		Pop = NULL;
	}
FUNC_MOCK_END

/*
 * pmemobj_pool_by_ptr -- pmemobj_pool_by_ptr mock
 *
 * Just return Pop.
 */
FUNC_MOCK_RET_ALWAYS(pmemobj_pool_by_ptr, PMEMobjpool *, Pop, const void *ptr);

/*
 * pmemobj_direct -- pmemobj_direct mock
 */
FUNC_MOCK(pmemobj_direct, void *, PMEMoid oid)
	FUNC_MOCK_RUN_DEFAULT {
		return (void *)((uintptr_t)Pop + oid.off);
	}
FUNC_MOCK_END

FUNC_MOCK_RET_ALWAYS(pmemobj_pool_by_oid, PMEMobjpool *, Pop, PMEMoid oid);

/*
 * pmemobj_alloc_usable_size -- pmemobj_alloc_usable_size mock
 */
FUNC_MOCK(pmemobj_alloc_usable_size, size_t, PMEMoid oid)
	FUNC_MOCK_RUN_DEFAULT {
		size_t size = palloc_usable_size(
				&Pop->heap, oid.off - OOB_OFF);
		return size - OOB_OFF;
	}
FUNC_MOCK_END

/*
 * pmemobj_alloc -- pmemobj_alloc mock
 *
 * Allocates an object using pmalloc and return PMEMoid.
 */
FUNC_MOCK(pmemobj_alloc, int, PMEMobjpool *pop, PMEMoid *oidp,
		size_t size, uint64_t type_num,
		pmemobj_constr constructor, void *arg)
	FUNC_MOCK_RUN_DEFAULT {
		PMEMoid oid = {0, 0};
		oid.pool_uuid_lo = 0;
		pmalloc(NULL, &oid.off, size, 0, 0);
		if (oidp) {
			*oidp = oid;
			pmemops_persist(&Pop->p_ops, oidp, sizeof(*oidp));
		}
		return 0;
	}
FUNC_MOCK_END

/*
 * lane_hold -- lane_hold mock
 *
 * Returns pointer to list lane section. For other types returns error.
 */
FUNC_MOCK(lane_hold, unsigned, PMEMobjpool *pop, struct lane_section **section,
		enum lane_section_type type)
	FUNC_MOCK_RUN_DEFAULT {
		int ret = 0;
		if (type != LANE_SECTION_LIST) {
			ret = -1;
			*section = NULL;
		} else {
			ret = 0;
			*section = &Lane_section;
		}
		return ret;
	}
FUNC_MOCK_END

/*
 * lane_release -- lane_release mock
 *
 * Always returns success.
 */
FUNC_MOCK_RET_ALWAYS_VOID(lane_release, PMEMobjpool *pop);

/*
 * lane_recover_and_section_boot -- lane_recover_and_section_boot mock
 */
FUNC_MOCK(lane_recover_and_section_boot, int, PMEMobjpool *pop)
	FUNC_MOCK_RUN_DEFAULT {
		return Section_ops[LANE_SECTION_LIST]->recover(Pop,
				Lane_section.layout,
				sizeof(*Lane_section.layout));
	}
FUNC_MOCK_END

/*
 * redo_log_store_last -- redo_log_store_last mock
 */
FUNC_MOCK(redo_log_store_last, void, const struct redo_ctx *ctx,
		struct redo_log *redo, size_t index,
		uint64_t offset, uint64_t value)
	FUNC_MOCK_RUN_DEFAULT {
		switch (Redo_fail) {
		case FAIL_AFTER_FINISH:
			_FUNC_REAL(redo_log_store_last)(ctx,
					redo, index, offset, value);
			DONEW(NULL);
			break;
		case FAIL_BEFORE_FINISH:
			DONEW(NULL);
			break;
		default:
			_FUNC_REAL(redo_log_store_last)(ctx,
					redo, index, offset, value);
			break;
		}

	}
FUNC_MOCK_END

/*
 * redo_log_set_last -- redo_log_set_last mock
 */
FUNC_MOCK(redo_log_set_last, void, const struct redo_ctx *ctx,
		struct redo_log *redo, size_t index)
	FUNC_MOCK_RUN_DEFAULT {
		switch (Redo_fail) {
		case FAIL_AFTER_FINISH:
			_FUNC_REAL(redo_log_set_last)(ctx, redo, index);
			DONEW(NULL);
			break;
		case FAIL_BEFORE_FINISH:
			DONEW(NULL);
			break;
		default:
			_FUNC_REAL(redo_log_set_last)(ctx, redo, index);
			break;
		}

	}
FUNC_MOCK_END

/*
 * redo_log_process -- redo_log_process mock
 */
FUNC_MOCK(redo_log_process, void, const struct redo_ctx *ctx,
		struct redo_log *redo, size_t nentries)
		FUNC_MOCK_RUN_DEFAULT {
			_FUNC_REAL(redo_log_process)(ctx, redo, nentries);
			if (Redo_fail == FAIL_AFTER_PROCESS) {
				DONEW(NULL);
			}
		}
FUNC_MOCK_END

/*
 * heap_boot -- heap_boot mock
 *
 * Always returns success.
 */
FUNC_MOCK_RET_ALWAYS(heap_boot, int, 0, PMEMobjpool *pop);

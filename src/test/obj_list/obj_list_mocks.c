/*
 * Copyright 2015-2019, Intel Corporation
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

#include <inttypes.h>
#include "valgrind_internal.h"
#include "obj_list.h"
#include "set.h"

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
static int
obj_persist(void *ctx, const void *addr, size_t len, unsigned flags)
{
	PMEMobjpool *pop = (PMEMobjpool *)ctx;
	pop->persist_local(addr, len);

	return 0;
}

/*
 * obj_flush -- pmemobj version of pmem_flush w/o replication
 */
static int
obj_flush(void *ctx, const void *addr, size_t len, unsigned flags)
{
	PMEMobjpool *pop = (PMEMobjpool *)ctx;
	pop->flush_local(addr, len);

	return 0;
}

static uintptr_t Pool_addr;
static size_t Pool_size;

static void
obj_msync_nofail(const void *addr, size_t size)
{
	uintptr_t addr_ptrt = (uintptr_t)addr;

	/*
	 * Verify msynced range is in the last mapped file range. Useful for
	 * catching errors which normally would be caught only on Windows by
	 * win_mmap.c.
	 */
	if (addr_ptrt < Pool_addr || addr_ptrt >= Pool_addr + Pool_size ||
			addr_ptrt + size >= Pool_addr + Pool_size)
		UT_FATAL("<0x%" PRIxPTR ",0x%" PRIxPTR "> "
				"not in <0x%" PRIxPTR ",0x%" PRIxPTR "> range",
				addr_ptrt, addr_ptrt + size, Pool_addr,
				Pool_addr + Pool_size);

	if (pmem_msync(addr, size))
		UT_FATAL("!pmem_msync");
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

static void *
obj_memcpy(void *ctx, void *dest, const void *src, size_t len,
	unsigned flags)
{
	return pmem_memcpy(dest, src, len, flags);
}

static void *
obj_memset(void *ctx, void *ptr, int c, size_t sz, unsigned flags)
{
	return pmem_memset(ptr, c, sz, flags);
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
	Pool_addr = (uintptr_t)addr;
	Pool_size = size;

	Pop = (PMEMobjpool *)addr;
	Pop->addr = Pop;
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
		Pop->memcpy_local = pmem_memcpy;
		Pop->memset_local = pmem_memset;
	} else {
		Pop->persist_local = obj_msync_nofail;
		Pop->flush_local = obj_msync_nofail;
		Pop->drain_local = pmem_drain_nop;
		Pop->memcpy_local = pmem_memcpy;
		Pop->memset_local = pmem_memset;
	}

	Pop->p_ops.persist = obj_persist;
	Pop->p_ops.flush = obj_flush;
	Pop->p_ops.drain = obj_drain;
	Pop->p_ops.memcpy = obj_memcpy;
	Pop->p_ops.memset = obj_memset;
	Pop->p_ops.base = Pop;
	struct pmem_ops *p_ops = &Pop->p_ops;

	Pop->heap_offset = HEAP_OFFSET;
	Pop->heap_size = size - Pop->heap_offset;
	uint64_t heap_offset = HEAP_OFFSET;

	Heap_offset = (uint64_t *)((uintptr_t)Pop +
			linear_alloc(&heap_offset, sizeof(*Heap_offset)));

	Id = (int *)((uintptr_t)Pop + linear_alloc(&heap_offset, sizeof(*Id)));

	/* Alloc lane layout */
	Lane.layout = (struct lane_layout *)((uintptr_t)Pop +
			linear_alloc(&heap_offset, LANE_TOTAL_SIZE));

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

	Lane.external = operation_new((struct ulog *)&Lane.layout->external,
		LANE_REDO_EXTERNAL_SIZE, NULL, NULL, p_ops, LOG_TYPE_REDO);

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
		operation_delete(Lane.external);
		UT_ASSERTeq(pmem_unmap(Pop,
			Pop->heap_size + Pop->heap_offset), 0);
		Pop = NULL;
		Pool_addr = 0;
		Pool_size = 0;
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
		pmalloc(pop, &oid.off, size, 0, 0);
		if (oidp) {
			*oidp = oid;
			if (OBJ_PTR_FROM_POOL(pop, oidp))
				pmemops_persist(&Pop->p_ops, oidp,
						sizeof(*oidp));
		}
		return 0;
	}
FUNC_MOCK_END

/*
 * lane_hold -- lane_hold mock
 *
 * Returns pointer to list lane section.
 */
FUNC_MOCK(lane_hold, unsigned, PMEMobjpool *pop, struct lane **lane)
	FUNC_MOCK_RUN_DEFAULT {
		*lane = &Lane;
		return 0;
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
		ulog_recover((struct ulog *)&Lane.layout->external,
			OBJ_OFF_IS_VALID_FROM_CTX, &pop->p_ops);
		return 0;
	}
FUNC_MOCK_END

/*
 * lane_section_cleanup -- lane_section_cleanup mock
 */
FUNC_MOCK(lane_section_cleanup, int, PMEMobjpool *pop)
	FUNC_MOCK_RUN_DEFAULT {
		return 0;
	}
FUNC_MOCK_END

/*
 * ulog_store_last -- ulog_store_last mock
 */
FUNC_MOCK(ulog_store, void,
	struct ulog *dest,
	struct ulog *src, size_t nbytes, size_t redo_base_nbytes,
	struct ulog_next *next, const struct pmem_ops *p_ops)
	FUNC_MOCK_RUN_DEFAULT {
		switch (Ulog_fail) {
		case FAIL_AFTER_FINISH:
			_FUNC_REAL(ulog_store)(dest, src,
					nbytes, redo_base_nbytes,
					next, p_ops);
			DONEW(NULL);
			break;
		case FAIL_BEFORE_FINISH:
			DONEW(NULL);
			break;
		default:
			_FUNC_REAL(ulog_store)(dest, src,
					nbytes, redo_base_nbytes,
					next, p_ops);
			break;
		}

	}
FUNC_MOCK_END

/*
 * ulog_process -- ulog_process mock
 */
FUNC_MOCK(ulog_process, void, struct ulog *ulog,
	ulog_check_offset_fn check, const struct pmem_ops *p_ops)
		FUNC_MOCK_RUN_DEFAULT {
			_FUNC_REAL(ulog_process)(ulog, check, p_ops);
			if (Ulog_fail == FAIL_AFTER_PROCESS) {
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

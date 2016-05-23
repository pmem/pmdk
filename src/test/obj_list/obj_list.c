/*
 * Copyright 2015-2016, Intel Corporation
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
 * obj_list.c -- unit tests for list module
 */

#include <stddef.h>
#include <sys/param.h>

#include "unittest.h"
#include "util.h"
#include "lane.h"
#include "redo.h"
#include "memops.h"
#include "pmalloc.h"
#include "list.h"
#include "pvector.h"
#include "obj.h"

/* offset to "in band" item */
#define OOB_OFF	 (sizeof(struct oob_header))
/* pmemobj initial heap offset */
#define HEAP_OFFSET	8192

TOID_DECLARE(struct item, 0);
TOID_DECLARE(struct list, 1);
TOID_DECLARE(struct oob_list, 2);
TOID_DECLARE(struct oob_item, 3);

struct item {
	int id;
	POBJ_LIST_ENTRY(struct item) next;
};

struct oob_item {
	struct oob_header oob;
	struct item item;
};

struct oob_list {
	struct list_head head;
};

struct list {
	POBJ_LIST_HEAD(listhead, struct item) head;
};

enum redo_fail
{
	/* don't fail at all */
	NO_FAIL,
	/* fail after redo_log_store_last or redo_log_set_last */
	FAIL_AFTER_FINISH,
	/* fail before redo_log_store_last or redo_log_set_last */
	FAIL_BEFORE_FINISH,
	/* fail after redo_log_process */
	FAIL_AFTER_PROCESS
};

/* global handle to pmemobj pool */
PMEMobjpool *Pop;
/* pointer to heap offset */
uint64_t *Heap_offset;
/* list lane section */
struct lane_section Lane_section;
/* actual item id */
int *Id;

/* fail event */
enum redo_fail Redo_fail = NO_FAIL;

/* global "in band" lists */
TOID(struct list) List;
TOID(struct list) List_sec;

/* global "out of band" lists */
TOID(struct oob_list) List_oob;
TOID(struct oob_list) List_oob_sec;

TOID(struct oob_item) *Item;

/* usage macros */
#define FATAL_USAGE()\
	UT_FATAL("usage: obj_list <file> [PRnifr]")
#define FATAL_USAGE_PRINT()\
	UT_FATAL("usage: obj_list <file> P:<list>")
#define FATAL_USAGE_PRINT_REVERSE()\
	UT_FATAL("usage: obj_list <file> R:<list>")
#define FATAL_USAGE_INSERT()\
	UT_FATAL("usage: obj_list <file> i:<where>:<num>")
#define FATAL_USAGE_REMOVE_FREE()\
	UT_FATAL("usage: obj_list <file> f:<list>:<num>:<from>")
#define FATAL_USAGE_REMOVE()\
	UT_FATAL("usage: obj_list <file> r:<num>")
#define FATAL_USAGE_MOVE()\
	UT_FATAL("usage: obj_list <file> m:<num>:<where>:<num>")
#define FATAL_USAGE_FAIL()\
	UT_FATAL("usage: obj_list <file> "\
	"F:<after_finish|before_finish|after_process>")

/*
 * pmem_drain_nop -- no operation for drain on non-pmem memory
 */
static void
pmem_drain_nop(void)
{
	/* nop */
}

/*
 * obj_persist -- pmemobj version of pmem_persist w/o replication
 */
static void
obj_persist(PMEMobjpool *pop, const void *addr, size_t len)
{
	pop->persist_local(addr, len);
}

/*
 * obj_flush -- pmemobj version of pmem_flush w/o replication
 */
static void
obj_flush(PMEMobjpool *pop, const void *addr, size_t len)
{
	pop->flush_local(addr, len);
}

/*
 * obj_drain -- pmemobj version of pmem_drain w/o replication
 */
static void
obj_drain(PMEMobjpool *pop)
{
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
 * pmemobj_open -- pmemobj_open mock
 *
 * This function initializes the pmemobj pool for purposes of this
 * unittest.
 */
FUNC_MOCK(pmemobj_open, PMEMobjpool *, char *fname, char *layout)
FUNC_MOCK_RUN_DEFAULT {
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

	if (Pop->is_pmem) {
		Pop->persist_local = pmem_persist;
		Pop->flush_local = pmem_flush;
		Pop->drain_local = pmem_drain;
	} else {
		Pop->persist_local = (persist_local_fn)pmem_msync;
		Pop->flush_local = (persist_local_fn)pmem_msync;
		Pop->drain_local = pmem_drain_nop;
	}

	Pop->persist = obj_persist;
	Pop->flush = obj_flush;
	Pop->drain = obj_drain;

	Pop->heap_offset = HEAP_OFFSET;
	Pop->heap_size = Pop->size - Pop->heap_offset;
	uint64_t heap_offset = HEAP_OFFSET;

	Heap_offset = (uint64_t *)((uintptr_t)Pop +
			linear_alloc(&heap_offset, sizeof(*Heap_offset)));

	Id = (int *)((uintptr_t)Pop + linear_alloc(&heap_offset, sizeof(*Id)));

	/* Alloc lane layout */
	Lane_section.layout = (void *)((uintptr_t)Pop +
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

	Item = (void *)((uintptr_t)Pop +
			linear_alloc(&heap_offset, sizeof(*Item)));
	Item->oid.pool_uuid_lo = Pop->uuid_lo;
	Item->oid.off = linear_alloc(&heap_offset, sizeof(struct oob_item));
	Pop->persist(Pop, Item, sizeof(*Item));

	if (*Heap_offset == 0) {
		*Heap_offset = heap_offset;
		Pop->persist(Pop, Heap_offset, sizeof(*Heap_offset));
	}

	Pop->persist(Pop, Pop, HEAP_OFFSET);

	Pop->run_id += 2;
	Pop->persist(Pop, &Pop->run_id, sizeof(Pop->run_id));

	return Pop;
}
FUNC_MOCK_END

/*
 * pmemobj_close -- pmemobj_close mock
 *
 * Just unmap the mapped area.
 */
FUNC_MOCK(pmemobj_close, void, PMEMobjpool *pop)
	_pobj_cached_pool.pop = NULL;
	_pobj_cached_pool.uuid_lo = 0;
	Pop = NULL;
	munmap(Pop, Pop->size);
FUNC_MOCK_END

int _pobj_cache_invalidate;
__thread struct _pobj_pcache _pobj_cached_pool;

FUNC_MOCK_RET_ALWAYS(pmemobj_pool_by_oid, PMEMobjpool *, Pop, PMEMoid oid);

/*
 * lane_hold -- lane_hold mock
 *
 * Returns pointer to list lane section. For other types returns error.
 */
FUNC_MOCK(lane_hold, int, PMEMobjpool *pop, struct lane_section **section,
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
FUNC_MOCK_RET_ALWAYS(lane_release, int, 0, PMEMobjpool *pop);

/*
 * heap_boot -- heap_boot mock
 *
 * Always returns success.
 */
FUNC_MOCK_RET_ALWAYS(heap_boot, int, 0, PMEMobjpool *pop);

/*
 * pmemobj_alloc -- pmemobj_alloc mock
 *
 * Allocates an object using pmalloc and return PMEMoid.
 */
FUNC_MOCK(pmemobj_alloc, PMEMoid, PMEMobjpool *pop, PMEMoid *oidp,
		size_t size, int type_num,
		void (*constructor)(PMEMobjpool *pop, void *ptr, void *arg),
		void *arg)
	FUNC_MOCK_RUN_DEFAULT {
		PMEMoid oid = {0, 0};
		oid.pool_uuid_lo = 0;
		pmalloc(NULL, &oid.off, size);
		if (oidp) {
			*oidp = oid;
			Pop->persist(Pop, oidp, sizeof(*oidp));
		}
	return oid; }
FUNC_MOCK_END

/*
 * pmalloc -- pmalloc mock
 *
 * Allocates the memory using linear allocator.
 * Prints the id of allocated struct oob_item for tracking purposes.
 */
FUNC_MOCK(pmalloc, int, PMEMobjpool *pop, uint64_t *ptr, size_t size)
	FUNC_MOCK_RUN_DEFAULT {
		size = 2 * (size - OOB_OFF) + OOB_OFF;
		uint64_t *alloc_size = (uint64_t *)((uintptr_t)Pop
				+ *Heap_offset);
		*alloc_size = size;
		Pop->persist(Pop, alloc_size, sizeof(*alloc_size));

		*ptr = *Heap_offset + sizeof(uint64_t);
		Pop->persist(Pop, ptr, sizeof(*ptr));

		struct oob_item *item =
			(struct oob_item *)((uintptr_t)Pop + *ptr);

		*ptr += OOB_OFF;
		item->item.id = *Id;
		Pop->persist(Pop, &item->item.id, sizeof(item->item.id));

		(*Id)++;
		Pop->persist(Pop, Id, sizeof(*Id));

		*Heap_offset = *Heap_offset + sizeof(uint64_t) +
			size + OOB_OFF;
		Pop->persist(Pop, Heap_offset, sizeof(*Heap_offset));

		UT_OUT("pmalloc(id = %d)", item->item.id);
		return 0;
	}
FUNC_MOCK_END

/*
 * pfree -- pfree mock
 *
 * Just prints freeing struct oob_item id. Doesn't free the memory.
 */
FUNC_MOCK(pfree, int, PMEMobjpool *pop, uint64_t *ptr)
	FUNC_MOCK_RUN_DEFAULT {
		struct oob_item *item =
			(struct oob_item *)((uintptr_t)Pop + *ptr - OOB_OFF);
		UT_OUT("pfree(id = %d)", item->item.id);
		*ptr = 0;
		Pop->persist(Pop, ptr, sizeof(*ptr));

		return 0;
	}
FUNC_MOCK_END

/*
 * pmalloc_construct -- pmalloc_construct mock
 *
 * Allocates the memory using linear allocator and invokes the constructor.
 * Prints the id of allocated struct oob_item for tracking purposes.
 */
FUNC_MOCK(pmalloc_construct, int, PMEMobjpool *pop, uint64_t *off,
	size_t size, void (*constructor)(PMEMobjpool *pop, void *ptr,
	size_t usable_size, void *arg), void *arg)
	FUNC_MOCK_RUN_DEFAULT {
		size = 2 * (size - OOB_OFF) + OOB_OFF;
		uint64_t *alloc_size = (uint64_t *)((uintptr_t)Pop +
				*Heap_offset);
		*alloc_size = size;
		Pop->persist(Pop, alloc_size, sizeof(*alloc_size));

		*off = *Heap_offset + sizeof(uint64_t) + OOB_OFF;
		Pop->persist(Pop, off, sizeof(*off));

		*Heap_offset = *Heap_offset + sizeof(uint64_t) + size;
		Pop->persist(Pop, Heap_offset, sizeof(*Heap_offset));

		void *ptr = (void *)((uintptr_t)Pop + *off);
		constructor(pop, ptr, size, arg);

		return 0;
	}
FUNC_MOCK_END

/*
 * prealloc -- prealloc mock
 */
FUNC_MOCK(prealloc, int, PMEMobjpool *pop, uint64_t *off, size_t size)
	FUNC_MOCK_RUN_DEFAULT {
		uint64_t *alloc_size = (uint64_t *)((uintptr_t)Pop +
				*off - sizeof(uint64_t));
		struct item *item = (struct item *)((uintptr_t)Pop +
				*off + OOB_OFF);
		if (*alloc_size >= size) {
			*alloc_size = size;
			Pop->persist(Pop, alloc_size, sizeof(*alloc_size));

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
	size_t size, void (*constructor)(PMEMobjpool *pop, void *ptr,
	size_t usable_size, void *arg), void *arg)
	FUNC_MOCK_RUN_DEFAULT {
		int ret = prealloc(pop, off, size);
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
FUNC_MOCK(pmalloc_usable_size, size_t, PMEMobjpool *pop, uint64_t off)
	FUNC_MOCK_RUN_DEFAULT {
		uint64_t *alloc_size = (uint64_t *)((uintptr_t)Pop +
				off - sizeof(uint64_t));
		return (size_t)*alloc_size;
	}
FUNC_MOCK_END

FUNC_MOCK(pmemobj_alloc_usable_size, size_t, PMEMoid oid)
	FUNC_MOCK_RUN_DEFAULT {
		size_t size = pmalloc_usable_size(Pop, oid.off - OOB_OFF);
		return size - OOB_OFF;
	}
FUNC_MOCK_END

/*
 * lane_recover_and_section_boot -- lane_recover_and_section_boot mock
 */
FUNC_MOCK(lane_recover_and_section_boot, int, PMEMobjpool *pop)
	FUNC_MOCK_RUN_DEFAULT {
		return Section_ops[LANE_SECTION_LIST]->recover(Pop,
				Lane_section.layout);
	}
FUNC_MOCK_END

/*
 * redo_log_store_last -- redo_log_store_last mock
 */
FUNC_MOCK(redo_log_store_last, void, PMEMobjpool *pop,
		struct redo_log *redo, size_t index,
		uint64_t offset, uint64_t value)
	FUNC_MOCK_RUN_DEFAULT {
		switch (Redo_fail) {
		case FAIL_AFTER_FINISH:
			_FUNC_REAL(redo_log_store_last)(pop,
					redo, index, offset, value);
			DONE(NULL);
			break;
		case FAIL_BEFORE_FINISH:
			DONE(NULL);
			break;
		default:
			_FUNC_REAL(redo_log_store_last)(pop,
					redo, index, offset, value);
			break;
		}

	}
FUNC_MOCK_END

/*
 * redo_log_set_last -- redo_log_set_last mock
 */
FUNC_MOCK(redo_log_set_last, void, PMEMobjpool *pop,
		struct redo_log *redo, size_t index)
	FUNC_MOCK_RUN_DEFAULT {
		switch (Redo_fail) {
		case FAIL_AFTER_FINISH:
			_FUNC_REAL(redo_log_set_last)(pop, redo, index);
			DONE(NULL);
			break;
		case FAIL_BEFORE_FINISH:
			DONE(NULL);
			break;
		default:
			_FUNC_REAL(redo_log_set_last)(pop, redo, index);
			break;
		}

	}
FUNC_MOCK_END

FUNC_MOCK(redo_log_process, void, PMEMobjpool *pop,
		struct redo_log *redo, size_t nentries)
		FUNC_MOCK_RUN_DEFAULT {
			_FUNC_REAL(redo_log_process)(pop, redo, nentries);
			if (Redo_fail == FAIL_AFTER_PROCESS) {
				DONE(NULL);
			}
		}
FUNC_MOCK_END

/*
 * for each element on list in normal order
 */
#define LIST_FOREACH(item, list, head, field)\
for ((item) = \
	D_RW((list))->head.pe_first;\
	!TOID_IS_NULL((item));\
	TOID_ASSIGN((item),\
	TOID_EQUALS((item),\
	D_RW(D_RW((list))->head.pe_first)->field.pe_prev) ?\
	OID_NULL : \
	D_RW(item)->field.pe_next.oid))

/*
 * for each element on list in reverse order
 */
#define LIST_FOREACH_REVERSE(item, list, head, field)\
for ((item) = \
	TOID_IS_NULL(D_RW((list))->head.pe_first) ? D_RW(list)->head.pe_first :\
	D_RW(D_RW(list)->head.pe_first)->field.pe_prev;\
	!TOID_IS_NULL((item));\
	TOID_ASSIGN((item),\
	TOID_EQUALS((item),\
	D_RW((list))->head.pe_first) ?\
	OID_NULL :\
	D_RW(item)->field.pe_prev.oid))

/*
 * get_item_list -- get nth item from list
 */
static PMEMoid
get_item_list(PMEMoid head, int n)
{
	TOID(struct list) list;
	TOID_ASSIGN(list, head);
	TOID(struct item) item;
	if (n >= 0) {
		LIST_FOREACH(item, list, head, next) {
			if (n == 0)
				return item.oid;
			n--;
		}
	} else {
		LIST_FOREACH_REVERSE(item, list, head, next) {
			n++;
			if (n == 0)
				return item.oid;
		}
	}

	return OID_NULL;
}

/*
 * do_print -- print list elements in normal order
 */
static void
do_print(PMEMobjpool *pop, const char *arg)
{
	int L;	/* which list */
	if (sscanf(arg, "P:%d", &L) != 1)
		FATAL_USAGE_PRINT();

	if (L == 2) {
		TOID(struct item) item;
		UT_OUT("list:");
		LIST_FOREACH(item, List, head, next) {
			UT_OUT("id = %d", D_RO(item)->id);
		}
	} else if (L == 4) {
		TOID(struct item) item;
		UT_OUT("list sec:");
		LIST_FOREACH(item, List_sec, head, next) {
			UT_OUT("id = %d", D_RO(item)->id);
		}
	} else {
		FATAL_USAGE_PRINT();
	}
}

/*
 * do_print_reverse -- print list elements in reverse order
 */
static void
do_print_reverse(PMEMobjpool *pop, const char *arg)
{
	int L;	/* which list */
	if (sscanf(arg, "R:%d", &L) != 1)
		FATAL_USAGE_PRINT_REVERSE();

	if (L == 2) {
		TOID(struct item) item;
		UT_OUT("list reverse:");
		LIST_FOREACH_REVERSE(item, List, head, next) {
			UT_OUT("id = %d", D_RO(item)->id);
		}
	} else if (L == 4) {
		TOID(struct item) item;
		UT_OUT("list sec reverse:");
		LIST_FOREACH_REVERSE(item, List_sec, head, next) {
			UT_OUT("id = %d", D_RO(item)->id);
		}
	} else {
		FATAL_USAGE_PRINT_REVERSE();
	}
}

/*
 * item_constructor -- constructor which sets the item's id to
 * new value
 */
static int
item_constructor(PMEMobjpool *pop, void *ptr, size_t usable_size, void *arg)
{
	int id = *(int *)arg;
	struct item *item = (struct item *)ptr;
	item->id = id;
	pop->persist(Pop, &item->id, sizeof(item->id));
	UT_OUT("constructor(id = %d)", id);

	return 0;
}

struct realloc_arg {
	void *ptr;
	size_t new_size;
	size_t old_size;
};

/*
 * do_insert_new -- insert new element to list
 */
static void
do_insert_new(PMEMobjpool *pop, const char *arg)
{
	int n;		/* which element on List */
	int before;
	int id;
	int ret = sscanf(arg, "n:%d:%d:%d", &before, &n, &id);
	if (ret == 3) {
		ret = list_insert_new_user(pop,
			offsetof(struct item, next),
			(struct list_head *)&D_RW(List)->head,
			get_item_list(List.oid, n),
			before,
			sizeof(struct item),
			item_constructor,
			&id, (PMEMoid *)Item);

		if (ret)
			UT_FATAL("list_insert_new(List, List_oob) failed");
	} else if (ret == 2) {
		ret = list_insert_new_user(pop,
			offsetof(struct item, next),
			(struct list_head *)&D_RW(List)->head,
			get_item_list(List.oid, n),
			before,
			sizeof(struct item),
			NULL, NULL, (PMEMoid *)Item);

		if (ret)
			UT_FATAL("list_insert_new(List, List_oob) failed");
	} else {
		ret = list_insert_new_user(pop,
			0, NULL, OID_NULL, 0,
			sizeof(struct item),
			NULL, NULL, (PMEMoid *)Item);

		if (ret)
			UT_FATAL("list_insert_new(List_oob) failed");
	}
}

/*
 * do_insert -- insert element to list
 */
static void
do_insert(PMEMobjpool *pop, const char *arg)
{
	int before;
	int n;	/* which element */
	if (sscanf(arg, "i:%d:%d",
			&before, &n) != 2)
		FATAL_USAGE_INSERT();

	PMEMoid it;
	pmemobj_alloc(pop, &it,
			sizeof(struct oob_item), 0, NULL, NULL);

	if (list_insert(pop,
		offsetof(struct item, next),
		(struct list_head *)&D_RW(List)->head,
		get_item_list(List.oid, n),
		before,
		it)) {
		UT_FATAL("list_insert(List) failed");
	}
}

/*
 * do_remove_free -- remove and free element from list
 */
static void
do_remove_free(PMEMobjpool *pop, const char *arg)
{
	int L;	/* which list */
	int n;	/* which element */
	int N;	/* remove from single/both lists */
	if (sscanf(arg, "f:%d:%d:%d", &L, &n, &N) != 3)
		FATAL_USAGE_REMOVE_FREE();

	PMEMoid oid;
	if (L == 2) {
		oid = get_item_list(List.oid, n);
	} else {
		FATAL_USAGE_REMOVE_FREE();
	}

	if (N == 1) {
		if (list_remove_free_user(pop,
			0,
			NULL,
			&oid)) {
			UT_FATAL("list_remove_free(List_oob) failed");
		}
	} else if (N == 2) {
		if (list_remove_free_user(pop,
			offsetof(struct item, next),
			(struct list_head *)&D_RW(List)->head,
			&oid)) {
			UT_FATAL("list_remove_free(List_oob, List) failed");
		}
	} else {
		FATAL_USAGE_REMOVE_FREE();
	}
}

/*
 * do_remove -- remove element from list
 */
static void
do_remove(PMEMobjpool *pop, const char *arg)
{
	int n;	/* which element */
	if (sscanf(arg, "r:%d", &n) != 1)
		FATAL_USAGE_REMOVE();

	if (list_remove(pop,
		offsetof(struct item, next),
		(struct list_head *)&D_RW(List)->head,
		get_item_list(List.oid, n))) {
		UT_FATAL("list_remove(List) failed");
	}
}

/*
 * do_move -- move element from one list to another
 */
static void
do_move(PMEMobjpool *pop, const char *arg)
{
	int n;
	int d;
	int before;
	if (sscanf(arg, "m:%d:%d:%d", &n, &before, &d) != 3)
		FATAL_USAGE_MOVE();

	if (list_move(pop,
		offsetof(struct item, next),
		(struct list_head *)&D_RW(List)->head,
		offsetof(struct item, next),
		(struct list_head *)&D_RW(List_sec)->head,
		get_item_list(List_sec.oid, d),
		before,
		get_item_list(List.oid, n))) {
		UT_FATAL("list_move(List, List_sec) failed");
	}
}

/*
 * do_move_one_list -- move element within one list
 */
static void
do_move_one_list(PMEMobjpool *pop, const char *arg)
{
	int n;
	int d;
	int before;
	if (sscanf(arg, "M:%d:%d:%d", &n, &before, &d) != 3)
		FATAL_USAGE_MOVE();

	if (list_move(pop,
		offsetof(struct item, next),
		(struct list_head *)&D_RW(List)->head,
		offsetof(struct item, next),
		(struct list_head *)&D_RW(List)->head,
		get_item_list(List.oid, d),
		before,
		get_item_list(List.oid, n))) {
		UT_FATAL("list_move(List, List) failed");
	}
}

/*
 * do_fail -- fail after specified event
 */
static void
do_fail(PMEMobjpool *pop, const char *arg)
{
	if (strcmp(arg, "F:before_finish") == 0) {
		Redo_fail = FAIL_BEFORE_FINISH;
	} else if (strcmp(arg, "F:after_finish") == 0) {
		Redo_fail = FAIL_AFTER_FINISH;
	} else if (strcmp(arg, "F:after_process") == 0) {
		Redo_fail = FAIL_AFTER_PROCESS;
	} else {
		FATAL_USAGE_FAIL();
	}
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_list");
	if (argc < 2)
		FATAL_USAGE();

	const char *path = argv[1];

	util_init(); /* to initialize On_valgrind flag */

	UT_COMPILE_ERROR_ON(OOB_OFF != 48);
	PMEMobjpool *pop = pmemobj_open(path, NULL);
	UT_ASSERTne(pop, NULL);

	UT_ASSERT(!TOID_IS_NULL(List));
	UT_ASSERT(!TOID_IS_NULL(List_oob));

	int i;
	for (i = 2; i < argc; i++) {
		switch (argv[i][0]) {
		case 'P':
			do_print(pop, argv[i]);
			break;
		case 'R':
			do_print_reverse(pop, argv[i]);
			break;
		case 'n':
			do_insert_new(pop, argv[i]);
			break;
		case 'i':
			do_insert(pop, argv[i]);
			break;
		case 'f':
			do_remove_free(pop, argv[i]);
			break;
		case 'r':
			do_remove(pop, argv[i]);
			break;
		case 'm':
			do_move(pop, argv[i]);
			break;
		case 'M':
			do_move_one_list(pop, argv[i]);
			break;
		case 'V':
			lane_recover_and_section_boot(pop);
			break;
		case 'F':
			do_fail(pop, argv[i]);
			break;
		default:
			FATAL_USAGE();
		}
	}

	pmemobj_close(pop);

	DONE(NULL);
}

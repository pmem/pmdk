// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2018, Intel Corporation */

/*
 * obj_list.c -- unit tests for list module
 */

/*
 * XXX - On VC++, this file must be compiled as C++ to have __typeof__ defined.
 * However, the rest of the files (libpmemobj source) are still compiled as C.
 * To avoid issues with 'C' linkage the entire file is in 'extern "C"' block.
 */
#if defined(__cplusplus) && defined(_MSC_VER)
extern "C" {

/*
 * XXX - Templates cannot be used with 'C' linkage, so for the purpose
 * of this test, we override template-based definition of __typeof__ with
 * a simple alias to decltype.
 */
#define __typeof__(p) decltype(p)

#endif

#include "obj_list.h"

/* global handle to pmemobj pool */
PMEMobjpool *Pop;
/* pointer to heap offset */
uint64_t *Heap_offset;
/* list lane section */
struct lane Lane;
/* actual item id */
int *Id;

/* fail event */
enum ulog_fail Ulog_fail = NO_FAIL;

/* global "in band" lists */
TOID(struct list) List;
TOID(struct list) List_sec;

/* global "out of band" lists */
TOID(struct oob_list) List_oob;
TOID(struct oob_list) List_oob_sec;

TOID(struct oob_item) *Item;

/*
 * for each element on list in normal order
 */
#define PLIST_FOREACH(item, list, head, field)\
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
#define PLIST_FOREACH_REVERSE(item, list, head, field)\
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
		PLIST_FOREACH(item, list, head, next) {
			if (n == 0)
				return item.oid;
			n--;
		}
	} else {
		PLIST_FOREACH_REVERSE(item, list, head, next) {
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
		PLIST_FOREACH(item, List, head, next) {
			UT_OUT("id = %d", D_RO(item)->id);
		}
	} else if (L == 4) {
		TOID(struct item) item;
		UT_OUT("list sec:");
		PLIST_FOREACH(item, List_sec, head, next) {
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
		PLIST_FOREACH_REVERSE(item, List, head, next) {
			UT_OUT("id = %d", D_RO(item)->id);
		}
	} else if (L == 4) {
		TOID(struct item) item;
		UT_OUT("list sec reverse:");
		PLIST_FOREACH_REVERSE(item, List_sec, head, next) {
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
item_constructor(void *ctx, void *ptr, size_t usable_size, void *arg)
{
	PMEMobjpool *pop = (PMEMobjpool *)ctx;
	int id = *(int *)arg;
	struct item *item = (struct item *)ptr;
	item->id = id;
	pmemops_persist(&pop->p_ops, &item->id, sizeof(item->id));
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
			TOID_TYPE_NUM(struct item),
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
			TOID_TYPE_NUM(struct item),
			NULL, NULL, (PMEMoid *)Item);

		if (ret)
			UT_FATAL("list_insert_new(List, List_oob) failed");
	} else {
		FATAL_USAGE_INSERT_NEW();
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
		Ulog_fail = FAIL_BEFORE_FINISH;
	} else if (strcmp(arg, "F:after_finish") == 0) {
		Ulog_fail = FAIL_AFTER_FINISH;
	} else if (strcmp(arg, "F:after_process") == 0) {
		Ulog_fail = FAIL_AFTER_PROCESS;
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

#if defined(__cplusplus) && defined(_MSC_VER)
}
#endif

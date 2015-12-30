/*
 * Copyright (c) 2015, Intel Corporation
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
 *     * Neither the name of Intel Corporation nor the names of its
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
 * obj_first_next.c -- unit tests for POBJ_FIRST macro
 */

#include <stddef.h>
#include "libpmemobj.h"
#include "unittest.h"

#define	LAYOUT_NAME "obj_first_next"

TOID_DECLARE(struct type, 0);
TOID_DECLARE(struct type_sec, 1);

struct type {
	int id;
};

struct type_sec {
	int id;
};

PMEMobjpool *pop;
typedef void (*fn_op)(int id);
typedef void (*fn_void)();

#define	FATAL_USAGE()\
	FATAL("usage: obj_first_next <file> [Parfn]")

/*
 * get_item_type -- get nth item from list
 */
static TOID(struct type)
get_item_type(int n)
{
	TOID(struct type) item;
	POBJ_FOREACH_TYPE(pop, item) {
		if (n == 0)
			return item;
		n--;
	}
	return TOID_NULL(struct type);
}

/*
 * get_item_type_sec -- get nth item from list
 */
static TOID(struct type_sec)
get_item_type_sec(int n)
{
	TOID(struct type_sec) item;
	POBJ_FOREACH_TYPE(pop, item) {
		if (n == 0)
			return item;
		n--;
	}
	return TOID_NULL(struct type_sec);
}

/*
 * do_print_type -- print list elements from type collection
 */
static void
do_print_type()
{
	TOID(struct type) item;
	OUT("type:");
	POBJ_FOREACH_TYPE(pop, item) {
		OUT("id = %d", D_RO(item)->id);
	}
}

/*
 * do_print_type_sec -- print list elements from type_sec collection
 */
static void
do_print_type_sec()
{
	TOID(struct type_sec) item;
	OUT("type_sec:");
	POBJ_FOREACH_TYPE(pop, item) {
		OUT("id = %d", D_RO(item)->id);
	}
}

fn_void do_print[] = {do_print_type, do_print_type_sec};

/*
 * type_constructor -- constructor which sets the item's id to
 * new value
 */
static void
type_constructor(PMEMobjpool *pop, void *ptr, void *arg)
{
	int id = *(int *)arg;
	struct type *item = (struct type *)ptr;
	item->id = id;
	OUT("constructor(id = %d)", id);
}

/*
 * type_sec_constructor -- constructor which sets the item's id to
 * new value
 */
static void
type_sec_constructor(PMEMobjpool *pop, void *ptr, void *arg)
{
	int id = *(int *)arg;
	struct type_sec *item = (struct type_sec *)ptr;
	item->id = id;
	OUT("constructor(id = %d)", id);
}

/*
 * do_alloc_type -- allocates new element to type collection
 */
static void
do_alloc_type(int id)
{
	TOID(struct type) item;
	POBJ_NEW(pop, &item, struct type, type_constructor, &id);
	if (TOID_IS_NULL(item))
		FATAL("POBJ_NEW");
}

/*
 * do_alloc_type_sec -- allocates new element to type_sec collection
 */
static void
do_alloc_type_sec(int id)
{
	TOID(struct type_sec) item;
	POBJ_NEW(pop, &item, struct type_sec, type_sec_constructor, &id);
	if (TOID_IS_NULL(item))
		FATAL("POBJ_NEW");
}

fn_op do_alloc[] = {do_alloc_type, do_alloc_type_sec};

/*
 * do_free_type -- remove and free element from type collection
 */
static void
do_free_type(int n)
{
	TOID(struct type) item;
	if (TOID_IS_NULL(POBJ_FIRST(pop, struct type)))
		return;
	item = get_item_type(n);
	ASSERT(!TOID_IS_NULL(item));
	POBJ_FREE(&item);
}

/*
 * do_free_type_sec -- remove and free element from type_sec collection
 */
static void
do_free_type_sec(int n)
{
	TOID(struct type_sec) item;
	if (TOID_IS_NULL(POBJ_FIRST(pop, struct type_sec)))
		return;
	item = get_item_type_sec(n);
	ASSERT(!TOID_IS_NULL(item));
	POBJ_FREE(&item);
}

fn_op do_free[] = {do_free_type, do_free_type_sec};

/*
 * do_first_type -- prints id of first object in type collection
 */
static void
do_first_type()
{
	TOID(struct type) first = POBJ_FIRST(pop, struct type);
	OUT("first id = %d", D_RO(first)->id);
}

/*
 * do_first_type_sec -- prints id of first object in type_sec collection
 */
static void
do_first_type_sec()
{
	TOID(struct type_sec) first = POBJ_FIRST(pop, struct type_sec);
	OUT("first id = %d", D_RO(first)->id);
}

fn_void do_first[] = {do_first_type, do_first_type_sec};

/*
 * do_next_type -- finds next element from type collection
 */
static void
do_next_type(int n)
{
	TOID(struct type) item;
	if (TOID_IS_NULL(POBJ_FIRST(pop, struct type)))
		return;
	item = get_item_type(n);
	ASSERT(!TOID_IS_NULL(item));
	item = POBJ_NEXT(item);
	OUT("next id = %d", D_RO(item)->id);
}

/*
 * do_next_type_sec -- finds next element from type_sec collection
 */
static void
do_next_type_sec(int n)
{
	TOID(struct type_sec) item;
	if (TOID_IS_NULL(POBJ_FIRST(pop, struct type_sec)))
		return;
	item = get_item_type_sec(n);
	ASSERT(!TOID_IS_NULL(item));
	item = POBJ_NEXT(item);
	OUT("next id = %d", D_RO(item)->id);
}

fn_op do_next[] = {do_next_type, do_next_type_sec};

/*
 * do_cleanup -- de-initialization function
 */
static void
do_cleanup()
{
	PMEMoid oid, oid_tmp;
	unsigned int type_num;
	POBJ_FOREACH_SAFE(pop, oid, oid_tmp, type_num)
		pmemobj_free(&oid);
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_first_next");
	if (argc < 2)
		FATAL_USAGE();

	const char *path = argv[1];
	if ((pop = pmemobj_create(path, LAYOUT_NAME, PMEMOBJ_MIN_POOL,
						S_IWUSR | S_IRUSR)) == NULL)
		FATAL("!pmemobj_create");

	for (int i = 2; i < argc; i++) {
		int list_num;
		int id;
		char type;
		sscanf(argv[i], "%c:%d:%d", &type, &list_num, &id);
		switch (type) {
		case 'P':
			do_print[list_num](id);
			break;
		case 'a':
			do_alloc[list_num](id);
			break;
		case 'r':
			do_free[list_num](id);
			break;
		case 'f':
			do_first[list_num](id);
			break;
		case 'n':
			do_next[list_num](id);
			break;
		default:
			FATAL_USAGE();
		}
	}
	do_cleanup();
	pmemobj_close(pop);

	DONE(NULL);
}

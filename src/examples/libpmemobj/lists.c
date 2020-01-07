// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016-2017, Intel Corporation */

/*
 * lists.c -- example usage of atomic lists API
 */

#include <ex_common.h>
#include <stdio.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <libpmemobj.h>

POBJ_LAYOUT_BEGIN(two_lists);
POBJ_LAYOUT_ROOT(two_lists, struct my_root);
POBJ_LAYOUT_TOID(two_lists, struct foo_el);
POBJ_LAYOUT_TOID(two_lists, struct bar_el);
POBJ_LAYOUT_END(two_lists);

#define MAX_LISTS 10

struct foo_el {
	POBJ_LIST_ENTRY(struct foo_el) entries;
	int value;
};

struct bar_el {
	POBJ_LIST_ENTRY(struct bar_el) entries;
	int value;
};

struct listbase {
	POBJ_LIST_HEAD(foo, struct foo_el) foo_list;
	POBJ_LIST_HEAD(bar, struct bar_el) bar_list;
};

struct my_root {
	struct listbase lists[MAX_LISTS];
};

enum list_type {
	LIST_INVALID,
	LIST_FOO,
	LIST_BAR,

	MAX_LIST_TYPES
};

struct list_constr_args {
	enum list_type type;
	int value;
};

/*
 * list_print -- prints the chosen list content to standard output
 */
static void
list_print(PMEMobjpool *pop, struct listbase *base, enum list_type type)
{
	switch (type) {
	case LIST_FOO: {
		TOID(struct foo_el) iter;
		POBJ_LIST_FOREACH(iter, &base->foo_list, entries) {
			printf("%d\n", D_RO(iter)->value);
		}
	} break;
	case LIST_BAR: {
		TOID(struct bar_el) iter;
		POBJ_LIST_FOREACH(iter, &base->bar_list, entries) {
			printf("%d\n", D_RO(iter)->value);
		}
	} break;
	default:
		assert(0);
	}
}

/*
 * list_element_constr -- constructor of the list element
 */
static int
list_element_constr(PMEMobjpool *pop, void *ptr, void *arg)
{
	struct list_constr_args *args = (struct list_constr_args *)arg;

	switch (args->type) {
	case LIST_FOO: {
		struct foo_el *e = (struct foo_el *)ptr;
		e->value = args->value;
		pmemobj_persist(pop, &e->value, sizeof(e->value));
	} break;
	case LIST_BAR: {
		struct bar_el *e = (struct bar_el *)ptr;
		e->value = args->value;
		pmemobj_persist(pop, &e->value, sizeof(e->value));
	} break;
	default:
		assert(0);
	}

	return 0;
}

/*
 * list_insert -- inserts a new element into the chosen list
 */
static void
list_insert(PMEMobjpool *pop, struct listbase *base,
	enum list_type type, int value)
{
	struct list_constr_args args = {type, value};

	switch (type) {
	case LIST_FOO:
		POBJ_LIST_INSERT_NEW_HEAD(pop, &base->foo_list, entries,
			sizeof(struct foo_el), list_element_constr, &args);
		break;
	case LIST_BAR:
		POBJ_LIST_INSERT_NEW_HEAD(pop, &base->bar_list, entries,
			sizeof(struct bar_el), list_element_constr, &args);
		break;
	default:
		assert(0);
	}

}

int
main(int argc, char *argv[])
{
	if (argc != 5) {
		printf("usage: %s file-name list_id foo|bar print|val\n",
			argv[0]);
		return 1;
	}

	const char *path = argv[1];

	PMEMobjpool *pop;

	if (file_exists(path) != 0) {
		if ((pop = pmemobj_create(path, POBJ_LAYOUT_NAME(two_lists),
			PMEMOBJ_MIN_POOL, 0666)) == NULL) {
			perror("failed to create pool\n");
			return 1;
		}
	} else {
		if ((pop = pmemobj_open(path,
				POBJ_LAYOUT_NAME(two_lists))) == NULL) {
			perror("failed to open pool\n");
			return 1;
		}
	}

	int id = atoi(argv[2]);
	if (id < 0 || id >= MAX_LISTS) {
		fprintf(stderr, "list index out of scope\n");
		return 1;
	}

	enum list_type type = LIST_INVALID;
	if (strcmp(argv[3], "foo") == 0) {
		type = LIST_FOO;
	} else if (strcmp(argv[3], "bar") == 0) {
		type = LIST_BAR;
	}

	if (type == LIST_INVALID) {
		fprintf(stderr, "invalid list type\n");
		return 1;
	}

	TOID(struct my_root) r = POBJ_ROOT(pop, struct my_root);
	if (strcmp(argv[4], "print") == 0) {
		list_print(pop, &D_RW(r)->lists[id], type);
	} else {
		int val = atoi(argv[4]);
		list_insert(pop, &D_RW(r)->lists[id], type, val);
	}

	pmemobj_close(pop);

	return 0;
}

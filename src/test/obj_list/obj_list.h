/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2015-2020, Intel Corporation */

/*
 * obj_list.h -- unit tests for list module
 */

#include <stddef.h>
#include <sys/param.h>

#include "list.h"
#include "obj.h"
#include "lane.h"
#include "unittest.h"
#include "util.h"

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

struct oob_header {
	char data[48];
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

enum ulog_fail
{
	/* don't fail at all */
	NO_FAIL,
	/* fail after ulog_store */
	FAIL_AFTER_FINISH,
	/* fail before ulog_store */
	FAIL_BEFORE_FINISH,
	/* fail after process */
	FAIL_AFTER_PROCESS
};

/* global handle to pmemobj pool */
extern PMEMobjpool *Pop;
/* pointer to heap offset */
extern uint64_t *Heap_offset;
/* list lane section */
extern struct lane Lane;
/* actual item id */
extern int *Id;

/* fail event */
extern enum ulog_fail Ulog_fail;

/* global "in band" lists */
extern TOID(struct list) List;
extern TOID(struct list) List_sec;

/* global "out of band" lists */
extern TOID(struct oob_list) List_oob;
extern TOID(struct oob_list) List_oob_sec;

extern TOID(struct oob_item) *Item;

/* usage macros */
#define FATAL_USAGE()\
	UT_FATAL("usage: obj_list <file> [PRnifr]")
#define FATAL_USAGE_PRINT()\
	UT_FATAL("usage: obj_list <file> P:<list>")
#define FATAL_USAGE_PRINT_REVERSE()\
	UT_FATAL("usage: obj_list <file> R:<list>")
#define FATAL_USAGE_INSERT()\
	UT_FATAL("usage: obj_list <file> i:<where>:<num>")
#define FATAL_USAGE_INSERT_NEW()\
	UT_FATAL("usage: obj_list <file> n:<where>:<num>:<value>")
#define FATAL_USAGE_REMOVE_FREE()\
	UT_FATAL("usage: obj_list <file> f:<list>:<num>:<from>")
#define FATAL_USAGE_REMOVE()\
	UT_FATAL("usage: obj_list <file> r:<num>")
#define FATAL_USAGE_MOVE()\
	UT_FATAL("usage: obj_list <file> m:<num>:<where>:<num>")
#define FATAL_USAGE_FAIL()\
	UT_FATAL("usage: obj_list <file> "\
	"F:<after_finish|before_finish|after_process>")

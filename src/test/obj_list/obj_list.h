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
extern PMEMobjpool *Pop;
/* pointer to heap offset */
extern uint64_t *Heap_offset;
/* list lane section */
extern struct lane_section Lane_section;
/* actual item id */
extern int *Id;

/* fail event */
extern enum redo_fail Redo_fail;

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

// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2018, Intel Corporation */

/*
 * list.h -- internal definitions for persistent atomic lists module
 */

#ifndef LIBPMEMOBJ_LIST_H
#define LIBPMEMOBJ_LIST_H 1

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#include "libpmemobj.h"
#include "lane.h"
#include "pmalloc.h"
#include "ulog.h"

#ifdef __cplusplus
extern "C" {
#endif

struct list_entry {
	PMEMoid pe_next;
	PMEMoid pe_prev;
};

struct list_head {
	PMEMoid pe_first;
	PMEMmutex lock;
};

int list_insert_new_user(PMEMobjpool *pop,
	size_t pe_offset, struct list_head *user_head, PMEMoid dest, int before,
	size_t size, uint64_t type_num, palloc_constr constructor, void *arg,
	PMEMoid *oidp);

int list_insert(PMEMobjpool *pop,
	ssize_t pe_offset, struct list_head *head, PMEMoid dest, int before,
	PMEMoid oid);

int list_remove_free_user(PMEMobjpool *pop,
	size_t pe_offset, struct list_head *user_head,
	PMEMoid *oidp);

int list_remove(PMEMobjpool *pop,
	ssize_t pe_offset, struct list_head *head,
	PMEMoid oid);

int list_move(PMEMobjpool *pop,
	size_t pe_offset_old, struct list_head *head_old,
	size_t pe_offset_new, struct list_head *head_new,
	PMEMoid dest, int before, PMEMoid oid);

void list_move_oob(PMEMobjpool *pop,
	struct list_head *head_old, struct list_head *head_new,
	PMEMoid oid);

#ifdef __cplusplus
}
#endif

#endif

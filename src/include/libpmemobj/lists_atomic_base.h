/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2014-2020, Intel Corporation */

/*
 * libpmemobj/lists_atomic_base.h -- definitions of libpmemobj atomic lists
 */

#ifndef LIBPMEMOBJ_LISTS_ATOMIC_BASE_H
#define LIBPMEMOBJ_LISTS_ATOMIC_BASE_H 1

#include <libpmemobj/base.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Non-transactional persistent atomic circular doubly-linked list
 */

int pmemobj_list_insert(PMEMobjpool *pop, size_t pe_offset, void *head,
	PMEMoid dest, int before, PMEMoid oid);

PMEMoid pmemobj_list_insert_new(PMEMobjpool *pop, size_t pe_offset, void *head,
	PMEMoid dest, int before, size_t size, uint64_t type_num,
	pmemobj_constr constructor, void *arg);

int pmemobj_list_remove(PMEMobjpool *pop, size_t pe_offset, void *head,
	PMEMoid oid, int free);

int pmemobj_list_move(PMEMobjpool *pop, size_t pe_old_offset,
	void *head_old, size_t pe_new_offset, void *head_new,
	PMEMoid dest, int before, PMEMoid oid);

#ifdef __cplusplus
}
#endif

#endif	/* libpmemobj/lists_atomic_base.h */

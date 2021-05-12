/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2015-2021, Intel Corporation */

/*
 * pmalloc.h -- internal definitions for persistent malloc
 */

#ifndef LIBPMEMOBJ_PMALLOC_H
#define LIBPMEMOBJ_PMALLOC_H 1

#include <stddef.h>
#include <stdint.h>

#include "libpmemobj.h"
#include "memops.h"
#include "palloc.h"

#ifdef __cplusplus
extern "C" {
#endif

/* single operations done in the internal context of the lane */

int pmalloc(PMEMobjpool *pop, uint64_t *off, size_t size,
	uint64_t extra_field, uint16_t object_flags);
int pmalloc_construct(PMEMobjpool *pop, uint64_t *off, size_t size,
	palloc_constr constructor, void *arg,
	uint64_t extra_field, uint16_t object_flags, uint16_t class_id);

int prealloc(PMEMobjpool *pop, uint64_t *off, size_t size,
	uint64_t extra_field, uint16_t object_flags);

void pfree(PMEMobjpool *pop, uint64_t *off);

/* external operation to be used together with context-aware palloc funcs */

struct operation_context *pmalloc_operation_hold(PMEMobjpool *pop);
struct operation_context *pmalloc_operation_hold_no_start(PMEMobjpool *pop);
void pmalloc_operation_release(PMEMobjpool *pop);

void pmalloc_ctl_register(PMEMobjpool *pop);

void pmalloc_global_ctl_register(void);

int pmalloc_cleanup(PMEMobjpool *pop);
int pmalloc_boot(PMEMobjpool *pop);

#ifdef __cplusplus
}
#endif

#endif

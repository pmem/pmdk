/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2017-2020, Intel Corporation */

/*
 * slab_allocator.h -- slab-like mechanism for libpmemobj
 */

#ifndef SLAB_ALLOCATOR_H
#define SLAB_ALLOCATOR_H

#include <libpmemobj.h>

struct slab_allocator;

struct slab_allocator *slab_new(PMEMobjpool *pop, size_t size);
void slab_delete(struct slab_allocator *slab);

int slab_alloc(struct slab_allocator *slab, PMEMoid *oid,
	pmemobj_constr constructor, void *arg);
PMEMoid slab_tx_alloc(struct slab_allocator *slab);

#endif /* SLAB_ALLOCATOR_H */

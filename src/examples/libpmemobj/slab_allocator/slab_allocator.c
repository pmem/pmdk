// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2017-2018, Intel Corporation */

/*
 * slab_allocator.c -- slab-like mechanism for libpmemobj
 */

#include "slab_allocator.h"
#include <stdlib.h>

struct slab_allocator {
	PMEMobjpool *pop;
	struct pobj_alloc_class_desc class;
};

/*
 * slab_new -- creates a new slab allocator instance
 */
struct slab_allocator *
slab_new(PMEMobjpool *pop, size_t size)
{
	struct slab_allocator *slab = malloc(sizeof(struct slab_allocator));
	if (slab == NULL)
		return NULL;

	slab->pop = pop;

	slab->class.header_type = POBJ_HEADER_NONE;
	slab->class.unit_size = size;
	slab->class.alignment = 0;

	/* should be a reasonably high number, but not too crazy */
	slab->class.units_per_block = 1000;

	if (pmemobj_ctl_set(pop,
		"heap.alloc_class.new.desc", &slab->class) != 0)
		goto error;

	return slab;

error:
	free(slab);
	return NULL;
}

/*
 * slab_delete -- deletes an existing slab allocator instance
 */
void
slab_delete(struct slab_allocator *slab)
{
	free(slab);
}

/*
 * slab_alloc -- works just like pmemobj_alloc but uses the predefined
 *	blocks from the slab
 */
int
slab_alloc(struct slab_allocator *slab, PMEMoid *oid,
	pmemobj_constr constructor, void *arg)
{
	return pmemobj_xalloc(slab->pop, oid, slab->class.unit_size, 0,
		POBJ_CLASS_ID(slab->class.class_id),
		constructor, arg);
}

/*
 * slab_tx_alloc -- works just like pmemobj_tx_alloc but uses the predefined
 *	blocks from the slab
 */
PMEMoid
slab_tx_alloc(struct slab_allocator *slab)
{
	return pmemobj_tx_xalloc(slab->class.unit_size, 0,
		POBJ_CLASS_ID(slab->class.class_id));
}

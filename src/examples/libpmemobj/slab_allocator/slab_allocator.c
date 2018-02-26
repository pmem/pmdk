/*
 * Copyright 2017-2018, Intel Corporation
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

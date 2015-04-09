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
 * pmalloc.c -- persistent malloc implementation
 */

#include <stdint.h>
#include <errno.h>
#include <stdlib.h>

#include "libpmemobj.h"
#include "pmalloc.h"


/*
 * heap_boot -- opens the heap region of the pmemobj pool
 *
 * If successful function returns zero. Otherwise an error number is returned.
 */
int
heap_boot(PMEMobjpool *pop)
{
	/* XXX */

	return 0;
}

/*
 * heap_init -- initializes the heap
 *
 * If successful function returns zero. Otherwise an error number is returned.
 */
int
heap_init(PMEMobjpool *pop)
{
	/* XXX */

	return 0;
}

/*
 * heap_boot -- cleanups the volatile heap state
 *
 * If successful function returns zero. Otherwise an error number is returned.
 */
int
heap_cleanup(PMEMobjpool *pop)
{
	/* XXX */

	return 0;
}

/*
 * heap_check -- verifies if the heap is consistent and can be opened properly
 *
 * If successful function returns zero. Otherwise an error number is returned.
 */
int
heap_check(PMEMobjpool *pop)
{
	/* XXX */

	return 0;
}

/*
 * pmalloc -- allocates a new block of memory
 *
 * The pool offset is written persistently into the off variable.
 *
 * If successful function returns zero. Otherwise an error number is returned.
 */
int
pmalloc(struct pmalloc_heap *heap, uint64_t *off, size_t size)
{
	/* XXX */

	return ENOSYS;
}

/*
 * pmalloc -- allocates a new block of memory with a constructor
 *
 * The block offset is written persistently into the off variable, but only
 * after the constructor function has been called.
 *
 * If successful function returns zero. Otherwise an error number is returned.
 */
int
pmalloc_construct(struct pmalloc_heap *heap, uint64_t *off, size_t size,
	void (*constructor)(void *ptr, void *arg))
{
	/* XXX */

	return ENOSYS;
}

/*
 * prealloc -- resizes an existing memory block previously allocated by pmalloc
 *
 * The block offset is written persistently into the off variable.
 *
 * If successful function returns zero. Otherwise an error number is returned.
 */
int
prealloc(struct pmalloc_heap *heap, uint64_t *off, size_t size)
{
	/* XXX */

	return ENOSYS;
}

/*
 * prealloc_construct -- resizes an existing memory block with a constructor
 *
 * The block offset is written persistently into the off variable, but only
 * after the constructor function has been called.
 *
 * If successful function returns zero. Otherwise an error number is returned.
 */
int
prealloc_construct(struct pmalloc_heap *heap, uint64_t *off, size_t size,
	void (*constructor)(void *ptr, void *arg))
{
	/* XXX */

	return ENOSYS;
}

/*
 * pmalloc_usable_size -- returns the number of bytes in the memory block
 */
size_t
pmalloc_usable_size(struct pmalloc_heap *heap, uint64_t off)
{
	/* XXX */

	return 0;
}

/*
 * pfree -- deallocates a memory block previously allocated by pmalloc
 *
 * A zero value is written persistently into the off variable.
 *
 * If successful function returns zero. Otherwise an error number is returned.
 */
int
pfree(struct pmalloc_heap *heap, uint64_t *off)
{
	/* XXX */

	return ENOSYS;
}

/*
 * pgrow -- grows in-place a memory block previously allocated by pmalloc
 *
 * If successful function returns zero. Otherwise an error number is returned.
 */
int
pgrow(struct pmalloc_heap *heap, uint64_t off, size_t size)
{
	/* XXX */

	return ENOSYS;
}

/*
 * Copyright 2015-2018, Intel Corporation
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
 * bucket.c -- bucket implementation
 *
 * Buckets manage volatile state of the heap. They are the abstraction layer
 * between the heap-managed chunks/runs and memory allocations.
 *
 * Each bucket instance can have a different underlying container that is
 * responsible for selecting blocks - which means that whether the allocator
 * serves memory blocks in best/first/next -fit manner is decided during bucket
 * creation.
 */

#include "alloc_class.h"
#include "bucket.h"
#include "heap.h"
#include "out.h"
#include "sys_util.h"
#include "valgrind_internal.h"

/*
 * bucket_new -- creates a new bucket instance
 */
struct bucket *
bucket_new(struct block_container *c, struct alloc_class *aclass)
{
	if (c == NULL)
		return NULL;

	struct bucket *b = Malloc(sizeof(*b));
	if (b == NULL)
		return NULL;

	b->container = c;
	b->c_ops = c->c_ops;

	util_mutex_init(&b->lock);

	b->is_active = 0;
	b->active_memory_block = NULL;
	if (aclass && aclass->type == CLASS_RUN) {
		b->active_memory_block =
			Zalloc(sizeof(struct memory_block_reserved));

		if (b->active_memory_block == NULL)
			goto error_active_alloc;
	}
	b->aclass = aclass;

	return b;

error_active_alloc:

	util_mutex_destroy(&b->lock);
	Free(b);
	return NULL;
}

/*
 * bucket_insert_block -- inserts a block into the bucket
 */
int
bucket_insert_block(struct bucket *b, const struct memory_block *m)
{
#if VG_MEMCHECK_ENABLED || VG_HELGRIND_ENABLED || VG_DRD_ENABLED
	if (On_valgrind) {
		size_t size = m->m_ops->get_real_size(m);
		void *data = m->m_ops->get_real_data(m);
		VALGRIND_DO_MAKE_MEM_NOACCESS(data, size);
		VALGRIND_ANNOTATE_NEW_MEMORY(data, size);
	}
#endif
	return b->c_ops->insert(b->container, m);
}

/*
 * bucket_delete -- cleanups and deallocates bucket instance
 */
void
bucket_delete(struct bucket *b)
{
	if (b->active_memory_block)
		Free(b->active_memory_block);

	util_mutex_destroy(&b->lock);
	b->c_ops->destroy(b->container);
	Free(b);
}

/*
 * bucket_current_resvp -- returns the pointer to the current reservation count
 */
int *
bucket_current_resvp(struct bucket *b)
{
	return b->active_memory_block ? &b->active_memory_block->nresv : NULL;
}

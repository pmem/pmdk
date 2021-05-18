// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2021, Intel Corporation */

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
#include "memblock.h"
#include "out.h"
#include "sys_util.h"
#include "valgrind_internal.h"

struct bucket {
	/* this struct is both the lock guard and the locked state */
	struct bucket_locked *locked;

	struct alloc_class *aclass;

	struct block_container *container;
	const struct block_container_ops *c_ops;

	struct memory_block_reserved *active_memory_block;
	int is_active;
};

struct bucket_locked {
	struct bucket bucket;
	os_mutex_t lock;
};

/*
 * bucket_init -- initalizes the bucket's runtime state
 */
static int
bucket_init(struct bucket *b, struct block_container *c,
	struct alloc_class *aclass)
{
	b->container = c;
	b->c_ops = c->c_ops;

	b->is_active = 0;
	b->active_memory_block = NULL;
	if (aclass && aclass->type == CLASS_RUN) {
		b->active_memory_block =
			Zalloc(sizeof(struct memory_block_reserved));

		if (b->active_memory_block == NULL)
			return -1;
	}
	b->aclass = aclass;

	return 0;
}

/*
 * bucket_fini -- destroys the bucket's runtime state
 */
static void
bucket_fini(struct bucket *b)
{
	if (b->active_memory_block)
		Free(b->active_memory_block);
	b->c_ops->destroy(b->container);
}

/*
 * bucket_locked_new -- creates a new locked bucket instance
 */
struct bucket_locked *
bucket_locked_new(struct block_container *c, struct alloc_class *aclass)
{
	ASSERTne(c, NULL);

	struct bucket_locked *b = Malloc(sizeof(*b));
	if (b == NULL)
		return NULL;

	if (bucket_init(&b->bucket, c, aclass) != 0)
		goto err_bucket_init;

	util_mutex_init(&b->lock);
	b->bucket.locked = b;

	return b;

err_bucket_init:
	Free(b);
	return NULL;
}

/*
 * bucket_locked_delete -- cleanups and deallocates locked bucket instance
 */
void
bucket_locked_delete(struct bucket_locked *b)
{
	bucket_fini(&b->bucket);
	util_mutex_destroy(&b->lock);
	Free(b);
}

/*
 * bucket_acquire -- acquires a usable bucket struct
 */
struct bucket *
bucket_acquire(struct bucket_locked *b)
{
	util_mutex_lock(&b->lock);
	return &b->bucket;
}

/*
 * bucket_release -- releases a bucket struct
 */
void
bucket_release(struct bucket *b)
{
	util_mutex_unlock(&b->locked->lock);
}

/*
 * bucket_try_insert_attached_block -- tries to return a previously allocated
 *	memory block back to the original bucket
 */
int
bucket_try_insert_attached_block(struct bucket *b, const struct memory_block *m)
{
	struct memory_block *active = &b->active_memory_block->m;

	if (b->is_active &&
	    m->chunk_id == active->chunk_id &&
	    m->zone_id == active->zone_id) {
		bucket_insert_block(b, m);
	}

	return 0;
}

/*
 * bucket_alloc_class -- returns the bucket's alloc class
 */
struct alloc_class *
bucket_alloc_class(struct bucket *b)
{
	return b->aclass;
}

/*
 * bucket_insert_block -- inserts a block into the bucket
 */
int
bucket_insert_block(struct bucket *b, const struct memory_block *m)
{
#if VG_MEMCHECK_ENABLED || VG_HELGRIND_ENABLED || VG_DRD_ENABLED
	if (On_memcheck || On_drd_or_hg) {
		size_t size = m->m_ops->get_real_size(m);
		void *data = m->m_ops->get_real_data(m);
		VALGRIND_DO_MAKE_MEM_NOACCESS(data, size);
		VALGRIND_ANNOTATE_NEW_MEMORY(data, size);
	}
#endif
	return b->c_ops->insert(b->container, m);
}

/*
 * bucket_remove_block -- removes an exact block from the bucket
 */
int
bucket_remove_block(struct bucket *b, const struct memory_block *m)
{
	return b->c_ops->get_rm_exact(b->container, m);
}

/*
 * bucket_alloc_block -- allocates a block from the bucket
 */
int
bucket_alloc_block(struct bucket *b, struct memory_block *m_out)
{
	return b->c_ops->get_rm_bestfit(b->container, m_out);
}

/*
 * bucket_memblock_insert_block -- (internal) bucket insert wrapper
 *	for callbacks
 */
static int
bucket_memblock_insert_block(const struct memory_block *m, void *b)
{
	return bucket_insert_block(b, m);
}

/*
 * bucket_attach_run - attaches a run to a bucket, making it active
 */
int
bucket_attach_run(struct bucket *b, const struct memory_block *m)
{
	os_mutex_t *lock = m->m_ops->get_lock(m);

	util_mutex_lock(lock);

	int ret = m->m_ops->iterate_free(m, bucket_memblock_insert_block, b);

	util_mutex_unlock(lock);

	if (ret == 0) {
		b->active_memory_block->m = *m;
		b->active_memory_block->bucket = b->locked;
		b->is_active = 1;
		util_fetch_and_add64(&b->active_memory_block->nresv, 1);
	} else {
		b->c_ops->rm_all(b->container);
	}
	return 0;
}

/*
 * bucket_attach_run - gets rid of the active block in the bucket
 */
int
bucket_detach_run(struct bucket *b, struct memory_block *m_out, int *empty)
{
	*empty = 0;

	struct memory_block_reserved **active = &b->active_memory_block;

	if (b->is_active) {
		b->c_ops->rm_all(b->container);
		if (util_fetch_and_sub64(&(*active)->nresv, 1) == 1) {
			*m_out = (*active)->m;
			*empty = 1;

			VALGRIND_ANNOTATE_HAPPENS_AFTER(&(*active)->nresv);
			(*active)->m = MEMORY_BLOCK_NONE;
		} else {
			VALGRIND_ANNOTATE_HAPPENS_BEFORE(&(*active)->nresv);
			*active = NULL;
		}
		b->is_active = 0;
	}

	if (*active == NULL) {
		*active = Zalloc(sizeof(struct memory_block_reserved));
		if (*active == NULL)
			return -1;
	}

	return 0;
}

/*
 * bucket_active_block -- returns the bucket active block
 */
struct memory_block_reserved *
bucket_active_block(struct bucket *b)
{
	return b->is_active ? b->active_memory_block : NULL;
}

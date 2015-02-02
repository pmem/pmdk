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
 * pmalloc.c -- implementation of pmalloc POSIX-like API
 */

#include <stdint.h>
#include <pthread.h>
#include <stdbool.h>
#include <assert.h>
#include <sys/param.h>
#include "pmalloc.h"
#include "out.h"
#include "util.h"
#include "bucket.h"
#include "arena.h"
#include "backend.h"
#include "pool.h"

/*
 * pool_open -- opens a new persistent pool
 */
struct pmalloc_pool *
pool_open(void *ptr, size_t size)
{
	LOG(3, "popen");

	return pool_new(ptr, size, BACKEND_PERSISTENT);
}

/*
 * pool_open_noop -- opens a new pool with no-op backend
 */
struct pmalloc_pool *
pool_open_noop(void *ptr, size_t size)
{
	LOG(3, "popen_noop");

	return pool_new(ptr, size, BACKEND_NOOP);
}

/*
 * pool_close -- closes a pool with any type of backend
 */
void
pool_close(struct pmalloc_pool *pool)
{
	LOG(3, "pclose");

	pool_delete(pool);
}

/*
 * alloc_from_bucket -- (internal) allocate an object from bucket
 */
static void
alloc_from_bucket(struct arena *arena, struct bucket *bucket,
	uint64_t *ptr, size_t size)
{
	uint64_t units = bucket_calc_units(bucket, size);

	/*
	 * Failure to find free object would mean that arena_select_bucket
	 * didn't work properly.
	 */
	struct bucket_object *obj = bucket_find_object(bucket, units);
	ASSERT(obj != NULL);

	arena->a_ops->set_alloc_ptr(arena, ptr, obj->data_offset);

	if (!bucket_remove_object(bucket, obj)) {
		LOG(4, "Failed to remove object from bucket");
		return;
	}
}

/*
 * pmalloc -- acquires a new object from pool
 *
 * The result of the operation is written persistently to the location
 * referenced by ptr. The *ptr value must be NULL_OFFSET.
 */
void
pmalloc(struct pmalloc_pool *p, uint64_t *ptr, size_t size)
{
	LOG(3, "pmalloc");
	ASSERT(*ptr == NULL_OFFSET);

	struct arena *arena = pool_select_arena(p);
	if (arena == NULL) {
		LOG(4, "Failed to select arena");
		return;
	}

	if (!arena_guard_up(arena, ptr, GUARD_TYPE_MALLOC)) {
		LOG(4, "Failed to acquire arena guard");
		return;
	}

	struct bucket *bucket = arena_select_bucket(arena, size);
	if (bucket == NULL) {
		LOG(4, "Failed to select a bucket, OOM");
		goto error_select_bucket;
	}

	alloc_from_bucket(arena, bucket, ptr, size);

error_select_bucket:
	if (!arena_guard_down(arena, ptr, GUARD_TYPE_MALLOC)) {
		LOG(4, "Failed to release arena guard");
		return;
	}
}

/*
 * pfree -- releases an object back to the pool
 *
 * If the operation is succesful a NULL is written persistently to the
 * location referenced by the ptr.
 */
void
pfree(struct pmalloc_pool *p, uint64_t *ptr)
{
	LOG(3, "pfree");
	if (*ptr == NULL_OFFSET)
		return;

	struct arena *arena = pool_select_arena(p);
	if (arena == NULL) {
		LOG(4, "Failed to select arena");
		return;
	}

	struct bucket_object obj;
	bucket_object_init(&obj, p, *ptr);

	if (!arena_guard_up(arena, ptr, GUARD_TYPE_FREE)) {
		LOG(4, "Failed to acquire arena guard");
		return;
	}

	if (!pool_recycle_object(p, &obj)) {
		LOG(4, "Failed to recycle object!");
		goto error_recycle_object;
	}

	arena->a_ops->set_alloc_ptr(arena, ptr, NULL_OFFSET);

error_recycle_object:
	if (!arena_guard_down(arena, ptr, GUARD_TYPE_FREE)) {
		LOG(4, "Failed to release arena guard");
		return;
	}
}

/*
 * prealloc - resizes or reacuqires an object from the pool
 */
void
prealloc(struct pmalloc_pool *p, uint64_t *ptr, size_t size)
{
	LOG(3, "prealloc");

	if (size == 0) {
		pfree(p, ptr);
		return;
	}

	if (*ptr == NULL_OFFSET) {
		pmalloc(p, ptr, size);
		return;
	}

	struct bucket_object obj;
	bucket_object_init(&obj, p, *ptr);

	if (obj.real_size >= size) {
		/* no-op */
		return;
	}

	struct arena *arena = pool_select_arena(p);
	if (arena == NULL) {
		LOG(4, "Failed to select arena");
		return;
	}

	if (!arena_guard_up(arena, ptr, GUARD_TYPE_REALLOC)) {
		LOG(4, "Failed to acquire arena guard");
		return;
	}

	struct bucket *bucket = arena_select_bucket(arena, size);
	if (bucket == NULL) {
		LOG(3, "Failed to select a bucket, OOM");
		goto error_select_bucket;
	}

	/* XXX object extend - after support for multiple buckets */

	/*
	 * Doing the following two operations in the reverse order would
	 * result in a short period of time in which there's no valid
	 * object stored in the ptr.
	 */
	alloc_from_bucket(arena, bucket, ptr, size);
	/* XXX memset */
	if (!pool_recycle_object(p, &obj)) {
		LOG(4, "Failed to recycle object!");
	}

error_select_bucket:
	if (!arena_guard_down(arena, ptr, GUARD_TYPE_REALLOC)) {
		LOG(4, "Failed to release arena guard");
		return;
	}
}

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
 * arena.c -- implementation of arena
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include "bucket.h"
#include "arena.h"
#include "backend.h"
#include "pool.h"
#include "pmalloc.h"
#include "out.h"
#include "util.h"

/*
 * arena_new -- allocate and initialize new arena object
 */
struct arena *
arena_new(struct pmalloc_pool *p, int arena_id)
{
	struct arena *arena = Malloc(sizeof (*arena));
	if (arena == NULL) {
		goto error_arena_malloc;
	}

	arena->lock = Malloc(sizeof (*arena->lock));
	if (arena->lock == NULL) {
		goto error_lock_malloc;
	}

	if (pthread_mutex_init(arena->lock, NULL) != 0) {
		goto error_lock_init;
	}

	arena->id = arena_id;
	arena->associated_threads = 0;
	arena->pool = p;
	arena->a_ops = p->backend->a_ops;
	memset(arena->buckets, 0, sizeof (*arena->buckets) * MAX_BUCKETS);

	return arena;
error_lock_init:
	Free(arena->lock);
error_lock_malloc:
	Free(arena);
error_arena_malloc:
	return NULL;
}

/*
 * arena_delete -- deinitialize and free arena object
 */
void
arena_delete(struct arena *a)
{
	for (int i = 0; i < MAX_BUCKETS; ++i) {
		if (a->buckets[i] != NULL) {
			bucket_delete(a->buckets[i]);
		}
	}

	if (pthread_mutex_destroy(a->lock) != 0) {
		LOG(4, "Failed to destroy arena lock");
	}
	Free(a->lock);
	Free(a);
}

/*
 * arena_guard_up -- acquire locks neccessery to perform operatin in threads
 */
bool
arena_guard_up(struct arena *arena, uint64_t *ptr, enum guard_type type)
{
	if (pthread_mutex_lock(arena->lock) != 0)
		return false;

	return true;
}

/*
 * arena_guard_down -- releases locks
 */
bool
arena_guard_down(struct arena *arena, uint64_t *ptr, enum guard_type type)
{
	if (pthread_mutex_unlock(arena->lock) != 0)
		return false;

	return true;
}

/*
 * arena_select_bucket -- returns a bucket for the object of a given size
 */
struct bucket *
arena_select_bucket(struct arena *arena, size_t size)
{
	int class_id = get_bucket_class_id_by_size(arena->pool, size);
	if (arena->buckets[class_id] == NULL) {
		arena->buckets[class_id] = bucket_new(arena->pool, class_id);
	}
	return arena->buckets[class_id];
}

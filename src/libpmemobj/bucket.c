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
 * bucket.c -- implementation of bucket
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include "bucket.h"
#include "arena.h"
#include "backend.h"
#include "pool.h"
#include "out.h"
#include "util.h"

/*
 * bucket_register_class - determines the bucket class for the size
 */
int
get_bucket_class_id_by_size(struct pmalloc_pool *p, size_t size)
{
	return 0;
}

/*
 * bucket_register_class - register a new bucket prototype class
 */
int
bucket_register_class(struct pmalloc_pool *p, struct bucket_class c)
{
	int i;
	for (i = 0; i < MAX_BUCKETS; ++i) {
		if (p->bucket_classes[i].unit_size == 0) {
			p->bucket_classes[i] = c;
			return i;
		}
	}

	return -1;
}

/*
 * bucket_unregister_class - unregister a bucket class
 *
 * This function does NOT affect existing buckets.
 */
bool
bucket_unregister_class(struct pmalloc_pool *p, int class_id)
{
	if (p->bucket_classes[class_id].unit_size == 0)
		return false;

	struct bucket_class empty = {0};
	p->bucket_classes[class_id] = empty;

	return true;
}

/*
 * bucket_new -- allocate and initialize new bucket object
 */
struct bucket *
bucket_new(struct pmalloc_pool *p, int class_id)
{
	struct bucket *bucket = Malloc(sizeof (*bucket));
	if (bucket == NULL) {
		goto error_bucket_malloc;
	}

	/*
	 * This would mean the class is not registered, which should never
	 * happen assuming correct implementation.
	 */
	ASSERT(p->bucket_classes[class_id].unit_size != 0);

	bucket->class = p->bucket_classes[class_id];
	bucket->pool = p;
	bucket->b_ops = p->backend->b_ops;

	return bucket;

error_bucket_malloc:
	return NULL;
}

/*
 * bucket_delete -- deinitialize and free bucket object
 */
void
bucket_delete(struct bucket *bucket)
{
	Free(bucket);
}

/*
 * bucket_transfer_objects -- removes certain number of objects from the bucket
 *
 * Returns a NULL-terminated list of objects that can be then inserted into
 * another bucket.
 */
struct bucket_object **
bucket_transfer_objects(struct bucket *bucket)
{
	return NULL;
}

/*
 * bucket_object_init -- initializes a bucket object based on the pointer
 */
void
bucket_object_init(struct bucket_object *obj, struct pmalloc_pool *p,
	uint64_t ptr)
{

}

/*
 * bucket_calc_units -- calculates the number of units needed for size
 */
uint64_t
bucket_calc_units(struct bucket *bucket, size_t size)
{
	return 0;
}

/*
 * bucket_find_object -- returns an object with the required unit size
 */
struct bucket_object *
bucket_find_object(struct bucket *bucket, uint64_t units)
{
	return NULL;
}

/*
 * bucket_remove_object -- removes object from the bucket
 */
bool
bucket_remove_object(struct bucket *bucket, struct bucket_object *obj)
{
	return false;
}

/*
 * bucket_add_object -- adds object to the bucket
 */
bool
bucket_add_object(struct bucket *bucket, struct bucket_object *obj)
{
	return false;
}

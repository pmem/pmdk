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
 * bucket.h -- internal definitions for bucket
 */

#define	MAX_BUCKETS 1024

struct bucket_object {
	uint64_t real_size;
	uint64_t data_offset;
};

struct bucket_class {
	int unit_size;
};

struct bucket {
	struct bucket_class class;
	struct pmalloc_pool *pool;
	struct bucket_backend_operations *b_ops;
};

struct bucket *bucket_new(struct pmalloc_pool *p, int class_id);
void bucket_delete(struct bucket *bucket);
uint64_t bucket_calc_units(struct bucket *bucket, size_t size);
struct bucket_object *bucket_find_object(struct bucket *bucket,
	uint64_t units);
bool bucket_remove_object(struct bucket *bucket, struct bucket_object *obj);
bool bucket_add_object(struct bucket *bucket, struct bucket_object *obj);
int get_bucket_class_id_by_size(struct pmalloc_pool *p, size_t size);
int bucket_register_class(struct pmalloc_pool *p, struct bucket_class c);
bool bucket_unregister_class(struct pmalloc_pool *p, int class_id);

/* NULL-terminated array */
struct bucket_object **bucket_transfer_objects(struct bucket *bucket);

void bucket_object_init(struct bucket_object *obj, struct pmalloc_pool *p,
	uint64_t ptr);

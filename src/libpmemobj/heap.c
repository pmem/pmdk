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
 * heap.c -- heap implementation
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <pthread.h>
#include <errno.h>

#include "libpmem.h"
#include "libpmemobj.h"
#include "util.h"
#include "heap.h"
#include "redo.h"
#include "heap_layout.h"
#include "bucket.h"
#include "lane.h"
#include "out.h"
#include "list.h"
#include "obj.h"
#include "valgrind_internal.h"

#define	MAX_BUCKET_REFILL 2
#define	MAX_BUCKETS 6
#define	DEFAULT_BUCKET 5
#define	RUN_UNIT_MAX 4
#define	MAX_RUN_LOCKS 1024

struct {
	size_t unit_size;
	int unit_max;
} bucket_proto[MAX_BUCKETS];

#define	BIT_IS_CLR(a, i)	(!((a) & (1L << (i))))

struct pmalloc_heap {
	struct heap_layout *layout;
	struct bucket *buckets[MAX_BUCKETS];
	struct bucket **bucket_map;
	pthread_mutex_t run_locks[MAX_RUN_LOCKS];
	int max_zone;
	int zones_exhausted;
	int last_run_max_size;
};

/*
 * heap_get_layout -- (internal) returns pointer to the heap layout
 */
static struct heap_layout *
heap_get_layout(PMEMobjpool *pop)
{
	return (void *)pop + pop->heap_offset;
}

/*
 * heap_max_zone -- (internal) calculates how many zones can the heap fit
 */
static int
heap_max_zone(size_t size)
{
	int max_zone = 0;
	size -= sizeof (struct heap_header);

	while (size >= ZONE_MIN_SIZE) {
		max_zone++;
		size -= size <= ZONE_MAX_SIZE ? size : ZONE_MAX_SIZE;
	}

	return max_zone;
}

/*
 * get_zone_size_idx -- (internal) calculates zone size index
 */
static uint32_t
get_zone_size_idx(uint32_t zone_id, int max_zone, size_t heap_size)
{
	if (zone_id < max_zone - 1)
		return MAX_CHUNK;

	size_t zone_raw_size = heap_size - zone_id * ZONE_MAX_SIZE;

	zone_raw_size -= sizeof (struct zone);

	return zone_raw_size / CHUNKSIZE;
}

/*
 * heap_chunk_write_footer -- (internal) writes a chunk footer
 */
static void
heap_chunk_write_footer(struct chunk_header *hdr)
{
	if (hdr->size_idx == 1) /* that would overwrite the header */
		return;

	struct chunk_header f = *hdr;
	f.type = CHUNK_TYPE_FOOTER;
	*(hdr + hdr->size_idx - 1) = f;
	/* no need to persist, footers are recreated in heap_populate_buckets */
	VALGRIND_SET_CLEAN((hdr + hdr->size_idx - 1), sizeof (f));
}

/*
 * heap_chunk_init -- (internal) writes chunk header
 */
static void
heap_chunk_init(PMEMobjpool *pop, struct chunk_header *hdr,
	uint16_t type, uint32_t size_idx)
{
	struct chunk_header nhdr = {
		.type = type,
		.flags = 0,
		.size_idx = size_idx
	};
	*hdr = nhdr; /* write the entire header (8 bytes) at once */
	pop->persist(hdr, sizeof (*hdr));

	heap_chunk_write_footer(hdr);
}

/*
 * heap_zone_init -- (internal) writes zone's first chunk and header
 */
static void
heap_zone_init(PMEMobjpool *pop, uint32_t zone_id)
{
	struct zone *z = &pop->heap->layout->zones[zone_id];
	uint32_t size_idx = get_zone_size_idx(zone_id, pop->heap->max_zone,
			pop->heap_size);

	heap_chunk_init(pop, &z->chunk_headers[0], CHUNK_TYPE_FREE, size_idx);

	struct zone_header nhdr = {
		.size_idx = size_idx,
		.magic = ZONE_HEADER_MAGIC,
	};
	z->header = nhdr;  /* write the entire header (8 bytes) at once */
	pop->persist(&z->header, sizeof (z->header));
}

/*
 * heap_init_run -- (internal) creates a run based on a chunk
 */
static void
heap_init_run(PMEMobjpool *pop, struct bucket *b, struct chunk_header *hdr,
	struct chunk_run *run)
{
	run->block_size = bucket_unit_size(b);
	pop->persist(&run->block_size, sizeof (run->block_size));

	ASSERT(hdr->type == CHUNK_TYPE_FREE);
	memset(run->bitmap, 0, sizeof (run->bitmap));
	int max_allocs = RUNSIZE / run->block_size;
	int unused_bits = RUN_BITMAP_SIZE - max_allocs;
	int unused_values = unused_bits / BITS_PER_VALUE;
	for (int i = MAX_BITMAP_VALUES - unused_values;
		i < MAX_BITMAP_VALUES; ++i)
		run->bitmap[i] = ~0L;

	unused_bits -= (unused_values * BITS_PER_VALUE);

	if (unused_bits != 0)
		run->bitmap[MAX_BITMAP_VALUES - unused_values - 1] |=
			(((1L << unused_bits) - 1L) << (64 - unused_bits));

	pop->persist(run->bitmap, sizeof (run->bitmap));

	hdr->type = CHUNK_TYPE_RUN;
	pop->persist(hdr, sizeof (*hdr));
}

/*
 * heap_run_insert -- (internal) inserts and splits a block of memory into a run
 */
static void
heap_run_insert(struct bucket *b, uint32_t chunk_id, uint32_t zone_id,
		uint32_t size_idx, uint16_t block_off)
{
	size_t unit_max = bucket_unit_max(b);
	uint32_t off = 0;
	uint32_t size;
	do {
		size = size_idx > unit_max ? unit_max : size_idx;
		bucket_insert_block(b, chunk_id, zone_id, size,
							block_off + off);
		off += size;
		size_idx -= size;
	} while (size_idx != 0);
}

/*
 * heap_populate_run_bucket -- (internal) split bitmap into memory blocks
 */
static void
heap_populate_run_bucket(PMEMobjpool *pop, struct bucket *b,
	uint32_t chunk_id, uint32_t zone_id)
{
	struct pmalloc_heap *h = pop->heap;
	struct zone *z = &h->layout->zones[zone_id];
	struct chunk_header *hdr = &z->chunk_headers[chunk_id];
	struct chunk_run *run = (struct chunk_run *)&z->chunks[chunk_id];

	if (hdr->type != CHUNK_TYPE_RUN)
		heap_init_run(pop, b, hdr, run);

	ASSERT(hdr->size_idx == 1);
	ASSERT(bucket_unit_size(b) == run->block_size);

	uint16_t run_bits = RUNSIZE / run->block_size;
	ASSERT(run_bits < (MAX_BITMAP_VALUES * BITS_PER_VALUE));
	uint16_t block_off = 0;
	uint16_t block_size_idx = 0;
	for (int i = 0; i < MAX_BITMAP_VALUES; ++i) {
		uint64_t v = run->bitmap[i];
		block_off = BITS_PER_VALUE * i;
		if (v == 0) {
			heap_run_insert(b, chunk_id, zone_id,
				BITS_PER_VALUE, block_off);
			continue;
		} else if (v == ~0L) {
			continue;
		}

		for (int j = 0; j < BITS_PER_VALUE; ++j) {
			if (BIT_IS_CLR(v, j)) {
				block_size_idx++;
			} else if (block_size_idx != 0) {
				heap_run_insert(b, chunk_id, zone_id,
					block_size_idx,
					block_off - block_size_idx);
				block_size_idx = 0;
			}

			if ((block_off++) == run_bits) {
				i = MAX_BITMAP_VALUES;
				break;
			}
		}
	}
}

/*
 * heap_populate_buckets -- (internal) creates volatile state of memory blocks
 */
static void
heap_populate_buckets(PMEMobjpool *pop)
{
	struct pmalloc_heap *h = pop->heap;

	if (h->zones_exhausted == h->max_zone)
		return;

	uint32_t zone_id = h->zones_exhausted++;
	struct zone *z = &h->layout->zones[zone_id];

	if (z->header.magic != ZONE_HEADER_MAGIC)
		heap_zone_init(pop, zone_id);

	struct bucket *def_bucket = h->buckets[DEFAULT_BUCKET];

	for (uint32_t i = 0; i < z->header.size_idx; ) {
		struct chunk_header *hdr = &z->chunk_headers[i];
		heap_chunk_write_footer(hdr);

		if (hdr->type == CHUNK_TYPE_RUN) {
			struct chunk_run *run =
				(struct chunk_run *)&z->chunks[i];
			heap_populate_run_bucket(pop,
				h->bucket_map[run->block_size], i, zone_id);
		} else if (hdr->type == CHUNK_TYPE_FREE) {
			bucket_insert_block(def_bucket, i, zone_id,
				hdr->size_idx, 0);
		}

		i += hdr->size_idx;
	}
}

/*
 * heap_ensure_bucket_filled -- (internal) refills the bucket if needed
 */
static void
heap_ensure_bucket_filled(PMEMobjpool *pop, struct bucket *b, int force)
{
	if (!force && !bucket_is_empty(b))
		return;

	if (!bucket_is_small(b)) {
		/* not much to do here apart from using the next zone */
		heap_populate_buckets(pop);
		return;
	}

	struct bucket *def_bucket = heap_get_default_bucket(pop);

	uint32_t chunk_id = 0;
	uint32_t zone_id = 0;
	uint32_t size_idx = 1;
	uint16_t block_off = 0;
	if (heap_get_block_from_bucket(pop, def_bucket, &chunk_id, &zone_id,
		size_idx, &block_off) != 0)
		return; /* OOM */

	ASSERT(block_off == 0);

	heap_populate_run_bucket(pop, b, chunk_id, zone_id);
}

/*
 * heap_get_default_bucket -- returns the bucket with CHUNKSIZE unit size
 */
struct bucket *
heap_get_default_bucket(PMEMobjpool *pop)
{
	struct bucket *b = pop->heap->buckets[DEFAULT_BUCKET];

	heap_ensure_bucket_filled(pop, b, 0);
	return b;
}

/*
 * heap_get_best_bucket -- returns the bucket that best fits the requested size
 */
struct bucket *
heap_get_best_bucket(PMEMobjpool *pop, size_t size)
{
	struct bucket *b = size < pop->heap->last_run_max_size ?
		pop->heap->bucket_map[size] :
		pop->heap->buckets[DEFAULT_BUCKET];

	heap_ensure_bucket_filled(pop, b, 0);
	return b;
}

/*
 * heap_buckets_init -- (internal) initializes bucket instances
 */
static int
heap_buckets_init(PMEMobjpool *pop)
{
	struct pmalloc_heap *h = pop->heap;
	int i;

	bucket_proto[0].unit_max = RUN_UNIT_MAX;

	/*
	 * To take use of every single bit available in the run the unit size
	 * would have to be calculated using following expression:
	 * (RUNSIZE / (MAX_BITMAP_VALUES * BITS_PER_VALUE)), but to preserve
	 * cacheline alignment a little bit of memory at the end of the run
	 * is left unused.
	 */
	bucket_proto[0].unit_size = MIN_RUN_SIZE;

	for (i = 1; i < MAX_BUCKETS - 1; ++i) {
		bucket_proto[i].unit_max = RUN_UNIT_MAX;
		bucket_proto[i].unit_size =
				bucket_proto[i - 1].unit_size *
				bucket_proto[i - 1].unit_max;
	}

	bucket_proto[i].unit_max = -1;
	bucket_proto[i].unit_size = CHUNKSIZE;

	h->last_run_max_size = bucket_proto[i - 1].unit_size *
				bucket_proto[i - 1].unit_max;
	h->bucket_map = Malloc(sizeof (*h->bucket_map) * h->last_run_max_size);
	if (h->bucket_map == NULL)
		goto error_bucket_map_malloc;

	for (i = 0; i < MAX_BUCKETS; ++i) {
		h->buckets[i] = bucket_new(bucket_proto[i].unit_size,
					bucket_proto[i].unit_max);
		if (h->buckets[i] == NULL)
			goto error_bucket_new;
	}

	/* XXX better way to fill the bucket map */
	for (i = 0; i < h->last_run_max_size; ++i) {
		for (int j = 0; j < MAX_BUCKETS - 1; ++j) {
			if ((bucket_proto[j].unit_size *
				(bucket_proto[j].unit_max)) > i) {
				h->bucket_map[i] = h->buckets[j];
				break;
			}
		}
	}

	heap_populate_buckets(pop);

	return 0;

error_bucket_new:
	Free(h->bucket_map);

	for (i = i - 1; i >= 0; --i)
		bucket_delete(h->buckets[i]);
error_bucket_map_malloc:

	return ENOMEM;
}

/*
 * heap_resize_chunk -- (internal) splits the chunk into two smaller ones
 */
static void
heap_resize_chunk(PMEMobjpool *pop,
	uint32_t chunk_id, uint32_t zone_id, uint32_t new_size_idx)
{
	uint32_t new_chunk_id = chunk_id + new_size_idx;

	struct zone *z = &pop->heap->layout->zones[zone_id];
	struct chunk_header *old_hdr = &z->chunk_headers[chunk_id];
	struct chunk_header *new_hdr = &z->chunk_headers[new_chunk_id];

	uint32_t rem_size_idx = old_hdr->size_idx - new_size_idx;
	heap_chunk_init(pop, new_hdr, CHUNK_TYPE_FREE, rem_size_idx);
	heap_chunk_init(pop, old_hdr, CHUNK_TYPE_FREE, new_size_idx);

	struct bucket *def_bucket = pop->heap->buckets[DEFAULT_BUCKET];
	if (bucket_insert_block(def_bucket, new_chunk_id, zone_id,
		rem_size_idx, 0) != 0) {
		LOG(1, "bucket_insert_block failed during resize");
	}
}

/*
 * heap_get_block_from_bucket -- extracts a memory block of equal size index
 */
int
heap_get_block_from_bucket(PMEMobjpool *pop, struct bucket *b,
	uint32_t *chunk_id, uint32_t *zone_id, uint32_t size_idx,
	uint16_t *block_off)
{
	if (bucket_lock(b) != 0)
		return EAGAIN;

	int i;
	uint32_t units = size_idx;
	for (i = 0; i < MAX_BUCKET_REFILL; ++i) {
		if (bucket_get_rm_block_bestfit(b, chunk_id, zone_id, &size_idx,
			block_off) != 0)
			heap_ensure_bucket_filled(pop, b, 1);
		else
			break;
	}

	if (i == MAX_BUCKET_REFILL) {
		bucket_unlock(b);
		return ENOMEM;
	}

	if (units != size_idx) {
		if (bucket_is_small(b))
			bucket_insert_block(b, *chunk_id, *zone_id,
				size_idx - units, *block_off + units);
		else
			heap_resize_chunk(pop, *chunk_id, *zone_id, units);
	}

	bucket_unlock(b);

	return 0;
}

/*
 * chunk_get_chunk_hdr_value -- (internal) get value of a header for redo log
 */
static uint64_t
chunk_get_chunk_hdr_value(struct chunk_header hdr, uint16_t type)
{
	uint64_t val;
	ASSERT(sizeof (struct chunk_header) == sizeof (uint64_t));

	hdr.type = type;
	memcpy(&val, &hdr, sizeof (val));

	return val;
}

/*
 * heap_get_block_header -- returns the header of the memory block
 */
void *
heap_get_block_header(PMEMobjpool *pop, uint32_t chunk_id,
	uint32_t zone_id, uint32_t size_idx, uint16_t block_off,
	enum heap_op op, uint64_t *op_result)
{
	struct zone *z = &pop->heap->layout->zones[zone_id];
	struct chunk_header *hdr = &z->chunk_headers[chunk_id];

	if (hdr->type != CHUNK_TYPE_RUN) {
		*op_result = chunk_get_chunk_hdr_value(*hdr, op == HEAP_OP_ALLOC
					? CHUNK_TYPE_USED : CHUNK_TYPE_FREE);
		return hdr;
	}

	struct chunk_run *r = (struct chunk_run *)&z->chunks[chunk_id];
	uint64_t bmask = ((1L << size_idx) - 1L) <<
			(block_off % BITS_PER_VALUE);

	int bpos = block_off / BITS_PER_VALUE;
	if (op == HEAP_OP_FREE) {
		*op_result = r->bitmap[bpos] & ~bmask;
	} else {
		*op_result = r->bitmap[bpos] | bmask;
	}

	return &r->bitmap[bpos];
}

/*
 * heap_get_block_data -- returns pointer to the data of a block
 */
void *
heap_get_block_data(PMEMobjpool *pop,
	uint32_t chunk_id, uint32_t zone_id, uint16_t block_off)
{
	struct zone *z = &pop->heap->layout->zones[zone_id];
	struct chunk_header *hdr = &z->chunk_headers[chunk_id];

	void *data = &z->chunks[chunk_id].data;
	if (hdr->type != CHUNK_TYPE_RUN)
		return data;

	struct chunk_run *run = data;
	ASSERT(run->block_size != 0);

	return (void *)&run->data + (run->block_size * block_off);
}

/*
 * heap_get_prev_chunk -- returns the header of the previous chunk
 */
struct chunk_header *
heap_get_prev_chunk(PMEMobjpool *pop, uint32_t *chunk_id,
	uint32_t zone_id)
{
	if (*chunk_id == 0)
		return NULL;

	struct zone *z = &pop->heap->layout->zones[zone_id];

	struct chunk_header *hdr = &z->chunk_headers[*chunk_id - 1];
	*chunk_id = *chunk_id - hdr->size_idx;

	return &z->chunk_headers[*chunk_id];
}

/*
 * heap_get_next_chunk -- returns the header of the next chunk
 */
struct chunk_header *
heap_get_next_chunk(PMEMobjpool *pop, uint32_t *chunk_id,
	uint32_t zone_id)
{
	struct zone *z = &pop->heap->layout->zones[zone_id];

	struct chunk_header *hdr = &z->chunk_headers[*chunk_id];
	if (*chunk_id + hdr->size_idx == z->header.size_idx)
		return NULL;

	*chunk_id = *chunk_id + hdr->size_idx;

	return &z->chunk_headers[*chunk_id];
}

/*
 * heap_coalesce -- merges neighbouring chunks into one
 */
struct chunk_header *
heap_coalesce(PMEMobjpool *pop, struct chunk_header *chunks[], int n)
{
	struct chunk_header *hdr = NULL;
	uint32_t size_idx = 0;
	for (int i = 0; i < n; ++i) {
		hdr = hdr ? : chunks[i];
		size_idx += chunks[i] ? chunks[i]->size_idx : 0;
	}

	heap_chunk_init(pop, hdr, CHUNK_TYPE_FREE, size_idx);

	return hdr;
}

/*
 * heap_lock_if_run -- acquire a run lock
 */
int
heap_lock_if_run(PMEMobjpool *pop, uint32_t chunk_id, uint32_t zone_id)
{
	struct chunk_header *hdr =
		&pop->heap->layout->zones[zone_id].chunk_headers[chunk_id];

	return hdr->type == CHUNK_TYPE_RUN ?
		pthread_mutex_lock(&pop->heap->run_locks[chunk_id %
						MAX_RUN_LOCKS]) : 0;
}

/*
 * heap_unlock_if_run -- release a run lock
 */
int
heap_unlock_if_run(PMEMobjpool *pop, uint32_t chunk_id, uint32_t zone_id)
{
	struct chunk_header *hdr =
		&pop->heap->layout->zones[zone_id].chunk_headers[chunk_id];

	return hdr->type == CHUNK_TYPE_RUN ?
		pthread_mutex_unlock(&pop->heap->run_locks[chunk_id %
						MAX_RUN_LOCKS]) : 0;
}

/*
 * heap_boot -- opens the heap region of the pmemobj pool
 *
 * If successful function returns zero. Otherwise an error number is returned.
 */
int
heap_boot(PMEMobjpool *pop)
{
	struct pmalloc_heap *h = Malloc(sizeof (*h));
	int err;
	if (h == NULL) {
		err = ENOMEM;
		goto error_heap_malloc;
	}

	h->max_zone = heap_max_zone(pop->heap_size);
	h->zones_exhausted = 0;
	h->layout = heap_get_layout(pop);
	for (int i = 0; i < MAX_RUN_LOCKS; ++i)
		if ((err = pthread_mutex_init(&h->run_locks[i], NULL)) != 0)
			goto error_run_lock_init;

	pop->heap = h;

	if ((err = heap_buckets_init(pop)) != 0)
		goto error_buckets_init;

	return 0;

error_buckets_init:
	/* there's really no point in destroying the locks */
error_run_lock_init:
	Free(h);
	pop->heap = NULL;
error_heap_malloc:
	return err;
}

/*
 * heap_write_header -- (internal) creates a clean header
 */
static void
heap_write_header(struct heap_header *hdr, size_t size)
{
	struct heap_header newhdr = {
		.signature = HEAP_SIGNATURE,
		.major = HEAP_MAJOR,
		.minor = HEAP_MINOR,
		.size = size,
		.chunksize = CHUNKSIZE,
		.chunks_per_zone = MAX_CHUNK,
		.reserved = {0},
		.checksum = 0
	};

	util_checksum(&newhdr, sizeof (newhdr), &newhdr.checksum, 1);
	*hdr = newhdr;
}

/*
 * heap_init -- initializes the heap
 *
 * If successful function returns zero. Otherwise an error number is returned.
 */
int
heap_init(PMEMobjpool *pop)
{
	if (pop->heap_size < HEAP_MIN_SIZE)
		return EINVAL;

	struct heap_layout *layout = heap_get_layout(pop);
	heap_write_header(&layout->header, pop->heap_size);
	pmem_msync(&layout->header, sizeof (struct heap_header));

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
	for (int i = 0; i < MAX_BUCKETS; ++i)
		bucket_delete(pop->heap->buckets[i]);

	for (int i = 0; i < MAX_RUN_LOCKS; ++i)
		pthread_mutex_destroy(&pop->heap->run_locks[i]);

	Free(pop->heap->bucket_map);

	Free(pop->heap);

	pop->heap = NULL;

	return 0;
}

/*
 * heap_verify_header -- (internal) verifies if the heap header is consistent
 */
static int
heap_verify_header(struct heap_header *hdr)
{
	if (util_checksum(hdr, sizeof (*hdr), &hdr->checksum, 0) != 1)
		return 1;

	if (memcmp(hdr->signature, HEAP_SIGNATURE, HEAP_SIGNATURE_LEN) != 0)
		return 1;

	return 0;
}

/*
 * heap_verify_zone_header --
 *	(internal) verifies if the zone header is consistent
 */
static int
heap_verify_zone_header(struct zone_header *hdr)
{
	if (hdr->size_idx == 0)
		return 1;

	return 0;
}

/*
 * heap_verify_chunk_header --
 *	(internal) verifies if the chunk header is consistent
 */
static int
heap_verify_chunk_header(struct chunk_header *hdr)
{
	if (hdr->type == CHUNK_TYPE_UNKNOWN)
		return 1;

	if (hdr->type >= MAX_CHUNK_TYPE)
		return 1;

	if (hdr->flags | ~(CHUNK_FLAG_ZEROED))
		return 1;

	return 0;
}

/*
 * heap_verify_zone -- (internal) verifies if the zone is consistent
 */
static int
heap_verify_zone(struct zone *zone)
{
	if (zone->header.magic != ZONE_HEADER_MAGIC)
		return 0; /* not initialized, and that is OK */

	if (heap_verify_zone_header(&zone->header))
		return 1;

	uint32_t i;
	for (i = 0; i < zone->header.size_idx; ) {
		if (heap_verify_chunk_header(&zone->chunk_headers[i]))
			return 1;

		i += zone->chunk_headers[i].size_idx;
	}

	if (i != zone->header.size_idx)
		return 1;

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
	if (pop->heap_size < HEAP_MIN_SIZE)
		return EINVAL;

	struct heap_layout *layout = heap_get_layout(pop);

	if (pop->heap_size != layout->header.size)
		return EINVAL;

	int ok = 0;
	ok &= heap_verify_header(&layout->header);
	for (int i = 0; i < heap_max_zone(layout->header.size); ++i) {
		ok &= heap_verify_zone(&layout->zones[i]);
	}

	return ok;
}

/*
 * Copyright 2016-2017, Intel Corporation
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
 * alloc_class.c -- implementation of allocation classes
 */

#include <float.h>
#include <string.h>

#include "alloc_class.h"
#include "heap_layout.h"
#include "util.h"
#include "out.h"
#include "bucket.h"

/*
 * Value used to mark a reserved spot in the bucket array.
 */
#define ACLASS_RESERVED ((void *)0xFFFFFFFFULL)

/*
 * The last size that is handled by runs.
 */
#define MAX_RUN_SIZE (CHUNKSIZE * 10)

/*
 * Maximum number of bytes the allocation class generation algorithm can decide
 * to waste in a single run chunk.
 */
#define MAX_RUN_WASTED_BYTES 1024

/*
 * Allocation categories are used for allocation classes generation. Each one
 * defines the biggest handled size (in bytes) and step pct of the generation
 * process. The step percentage defines maximum allowed external fragmentation
 * for the category.
 */
#define MAX_ALLOC_CATEGORIES 9

/*
 * The first size (in byes) which is actually used in the allocation
 * class generation algorithm. All smaller sizes use the first predefined bucket
 * with the smallest run unit size.
 */
#define FIRST_GENERATED_CLASS_SIZE 128

/*
 * The granularity of the allocation class generation algorithm.
 */
#define ALLOC_BLOCK_SIZE_GEN 64

static struct {
	size_t size;
	float step;
} categories[MAX_ALLOC_CATEGORIES] = {
	/* dummy category - the first allocation class is predefined */
	{FIRST_GENERATED_CLASS_SIZE, 0.05f},
	{1024, 0.05f},
	{2048, 0.05f},
	{4096, 0.05f},
	{8192, 0.05f},
	{16384, 0.05f},
	{32768, 0.05f},
	{131072, 0.05f},
	{393216, 0.05f},
};

#define RUN_UNIT_MAX_ALLOC 8U

/*
 * Every allocation has to be a multiple of at least 8 because we need to
 * ensure proper alignment of every pmem structure.
 */
#define ALLOC_BLOCK_SIZE 16

/*
 * Converts size (in bytes) to number of allocation blocks.
 */
#define SIZE_TO_CLASS_MAP_INDEX(_s, _g) (1 + (((_s) - 1) / (_g)))

/*
 * Calculates the size in bytes of a single run instance
 */
#define RUN_SIZE_BYTES(size_idx)\
(RUNSIZE + ((size_idx - 1) * CHUNKSIZE))

/*
 * Target number of allocations per run instance.
 */
#define RUN_MIN_NALLOCS 500

/*
 * Hard limit of chunks per single run.
 */
#define RUN_SIZE_IDX_CAP (16)

struct alloc_class_collection {
	size_t granularity;

	struct alloc_class *aclasses[MAX_ALLOCATION_CLASSES];

	/*
	 * The last size (in bytes) that is handled by runs, everything bigger
	 * uses the default class.
	 */
	size_t last_run_max_size;

	/* maps allocation classes to allocation sizes, excluding the header! */
	uint8_t *class_map_by_alloc_size;

	/* maps allocation classes to run unit sizes */
	uint8_t *class_map_by_unit_size;

	int fail_on_missing_class;
};

/*
 * alloc_class_find_first_free_slot -- searches for the
 *	first available allocation class slot
 *
 * This function must be thread-safe because allocation classes can be created
 * at runtime.
 */
int
alloc_class_find_first_free_slot(struct alloc_class_collection *ac,
	uint8_t *slot)
{
	for (int n = 0; n < MAX_ALLOCATION_CLASSES; ++n) {
		if (util_bool_compare_and_swap64(&ac->aclasses[n],
				NULL, ACLASS_RESERVED)) {
			*slot = (uint8_t)n;
			return 0;
		}
	}

	return -1;
}

/*
 * alloc_class_generate_run_proto -- generates the run bitmap-related
 *	information needed for the allocation class
 */
void
alloc_class_generate_run_proto(struct alloc_class_run_proto *dest,
	size_t unit_size, uint32_t size_idx)
{
	ASSERTne(size_idx, 0);
	dest->size_idx = size_idx;

	/*
	 * Here the bitmap definition is calculated based on the
	 * size of the available memory and the size of
	 * a memory block - the result of dividing those two
	 * numbers is the number of possible allocations from
	 * that block, and in other words, the amount of bits
	 * in the bitmap.
	 */
	dest->bitmap_nallocs = (uint32_t)
		(RUN_SIZE_BYTES(dest->size_idx) / unit_size);

	while (dest->bitmap_nallocs > RUN_BITMAP_SIZE) {
		LOG(3, "tried to create allocation class (%lu) with number "
			"of units (%u) exceeding the bitmap size (%u)",
			unit_size, dest->bitmap_nallocs, RUN_BITMAP_SIZE);
		if (dest->size_idx > 1) {
			dest->size_idx -= 1;
			/* recalculate the number of allocations */
			dest->bitmap_nallocs = (uint32_t)
				(RUN_SIZE_BYTES(dest->size_idx) / unit_size);
			LOG(3, "allocation class (%lu) was constructed with "
				"fewer (%u) than requested chunks (%u)",
				unit_size, dest->size_idx, dest->size_idx + 1);
		} else {
			LOG(3, "allocation class (%lu) was constructed with "
				"fewer units (%u) than optimal (%u), "
				"this might lead to "
				"inefficient memory utilization!",
				unit_size,
				RUN_BITMAP_SIZE, dest->bitmap_nallocs);

			dest->bitmap_nallocs = RUN_BITMAP_SIZE;
		}
	}

	/*
	 * The two other numbers that define our bitmap is the
	 * size of the array that represents the bitmap and the
	 * last value of that array with the bits that exceed
	 * number of blocks marked as set (1).
	 */
	ASSERT(dest->bitmap_nallocs <= RUN_BITMAP_SIZE);
	unsigned unused_bits = RUN_BITMAP_SIZE - dest->bitmap_nallocs;

	unsigned unused_values = unused_bits / BITS_PER_VALUE;

	ASSERT(MAX_BITMAP_VALUES >= unused_values);
	dest->bitmap_nval = MAX_BITMAP_VALUES - unused_values;

	ASSERT(unused_bits >= unused_values * BITS_PER_VALUE);
	unused_bits -= unused_values * BITS_PER_VALUE;

	dest->bitmap_lastval = unused_bits ?
		(((1ULL << unused_bits) - 1ULL) <<
			(BITS_PER_VALUE - unused_bits)) : 0;
}

/*
 * alloc_class_register -- registers an allocation classes in the collection
 */
struct alloc_class *
alloc_class_register(struct alloc_class_collection *ac,
	struct alloc_class *c)
{
	struct alloc_class *nc = Malloc(sizeof(*nc));
	if (nc == NULL)
		return NULL;

	*nc = *c;
	ac->class_map_by_unit_size[SIZE_TO_CLASS_MAP_INDEX(nc->unit_size,
		ac->granularity)] = nc->id;

	ac->aclasses[nc->id] = nc;

	return nc;
}

/*
 * alloc_class_from_params -- (internal) creates a new allocation class
 */
static struct alloc_class *
alloc_class_from_params(struct alloc_class_collection *ac,
	enum alloc_class_type type,
	size_t unit_size,
	unsigned unit_max, unsigned unit_max_alloc,
	uint32_t size_idx)
{
	struct alloc_class c;

	c.unit_size = unit_size;
	c.header_type = HEADER_COMPACT;
	c.type = type;

	switch (type) {
		case CLASS_HUGE:
			c.id = DEFAULT_ALLOC_CLASS_ID;
			break;
		case CLASS_RUN:
			alloc_class_generate_run_proto(&c.run, unit_size,
				size_idx);

			uint8_t slot;
			if (alloc_class_find_first_free_slot(ac, &slot) != 0) {
				return NULL;
			}

			c.id = slot;

			break;
		default:
			ASSERT(0);
	}

	return alloc_class_register(ac, &c);
}

/*
 * alloc_class_delete -- (internal) deletes an allocation class
 */
void
alloc_class_delete(struct alloc_class_collection *ac,
	struct alloc_class *c)
{
	ac->aclasses[c->id] = NULL;
	Free(c);
}

/*
 * alloc_class_find_or_create -- (internal) searches for the
 * biggest allocation class for which unit_size is evenly divisible by n.
 * If no such class exists, create one.
 */
static struct alloc_class *
alloc_class_find_or_create(struct alloc_class_collection *ac, size_t n)
{
	COMPILE_ERROR_ON(MAX_ALLOCATION_CLASSES > UINT8_MAX);
	uint64_t required_size_bytes = n * RUN_MIN_NALLOCS;
	uint32_t required_size_idx = 1;
	if (required_size_bytes > RUNSIZE) {
		required_size_bytes -= RUNSIZE;
		required_size_idx +=
			CALC_SIZE_IDX(CHUNKSIZE, required_size_bytes);
		if (required_size_idx > RUN_SIZE_IDX_CAP)
			required_size_idx = RUN_SIZE_IDX_CAP;
	}

	for (int i = MAX_ALLOCATION_CLASSES - 1; i >= 0; --i) {
		struct alloc_class *c = ac->aclasses[i];

		if (c == NULL || c->type == CLASS_HUGE ||
				c->run.size_idx < required_size_idx)
			continue;

		if (n % c->unit_size == 0 &&
			n / c->unit_size <= RUN_UNIT_MAX_ALLOC)
			return c;
	}

	/*
	 * In order to minimize the wasted space at the end of the run the
	 * run data size must be divisible by the allocation class unit size
	 * with the smallest possible remainder, preferably 0.
	 */
	size_t runsize_bytes = RUN_SIZE_BYTES(required_size_idx);
	while ((runsize_bytes % n) > MAX_RUN_WASTED_BYTES) {
		n += ALLOC_BLOCK_SIZE_GEN;
	}

	/*
	 * Now that the desired unit size is found the existing classes need
	 * to be searched for possible duplicates. If a class that can handle
	 * the calculated size already exists, simply return that.
	 */
	for (int i = 1; i < MAX_ALLOCATION_CLASSES; ++i) {
		struct alloc_class *c = ac->aclasses[i];
		if (c == NULL || c->type == CLASS_HUGE)
			continue;
		if (n / c->unit_size <= RUN_UNIT_MAX_ALLOC &&
			n % c->unit_size == 0)
			return c;
		if (c->unit_size == n)
			return c;
	}

	return alloc_class_from_params(ac, CLASS_RUN, n,
		RUN_UNIT_MAX, RUN_UNIT_MAX_ALLOC, required_size_idx);
}

/*
 * alloc_class_find_min_frag -- searches for an existing allocation
 * class that will provide the smallest internal fragmentation for the given
 * size.
 */
static struct alloc_class *
alloc_class_find_min_frag(struct alloc_class_collection *ac, size_t n)
{
	struct alloc_class *best_c = NULL;
	float best_frag = FLT_MAX;

	ASSERTne(n, 0);

	/*
	 * Start from the largest buckets in order to minimize unit size of
	 * allocated memory blocks.
	 */
	for (int i = MAX_ALLOCATION_CLASSES - 1; i >= 0; --i) {
		struct alloc_class *c = ac->aclasses[i];

		if (c == NULL)
			continue;

		size_t real_size = n + header_type_to_size[c->header_type];

		size_t units = CALC_SIZE_IDX(c->unit_size, real_size);
		/* can't exceed the maximum allowed run unit max */
		if (units > RUN_UNIT_MAX_ALLOC)
			break;

		float frag = (float)(c->unit_size * units) / (float)real_size;
		if (frag == 1.f)
			return c;

		ASSERT(frag >= 1.f);
		if (frag < best_frag || best_c == NULL) {
			best_c = c;
			best_frag = frag;
		}
	}

	ASSERTne(best_c, NULL);
	return best_c;
}

/*
 * alloc_class_collection_new -- creates a new collection of allocation classes
 */
struct alloc_class_collection *
alloc_class_collection_new()
{
	struct alloc_class_collection *ac = Malloc(sizeof(*ac));
	if (ac == NULL)
		return NULL;

	memset(ac->aclasses, 0, sizeof(ac->aclasses));

	ac->granularity = ALLOC_BLOCK_SIZE;
	ac->last_run_max_size = MAX_RUN_SIZE;
	ac->fail_on_missing_class = 0;

	size_t maps_size = (MAX_RUN_SIZE / ac->granularity) + 1;

	ac->class_map_by_alloc_size = Malloc(maps_size);
	ac->class_map_by_unit_size = Malloc(maps_size);
	memset(ac->class_map_by_alloc_size, 0xFF, maps_size);
	memset(ac->class_map_by_unit_size, 0xFF, maps_size);

	if (alloc_class_from_params(ac, CLASS_HUGE, CHUNKSIZE, 0, 0, 1) == NULL)
		goto error_alloc_class_create;

	struct alloc_class *predefined_class =
		alloc_class_from_params(ac, CLASS_RUN, MIN_RUN_SIZE,
			RUN_UNIT_MAX, RUN_UNIT_MAX_ALLOC, 1);
	if (predefined_class == NULL)
		goto error_alloc_class_create;

	for (size_t i = 0; i < FIRST_GENERATED_CLASS_SIZE / ac->granularity;
		++i) {
		ac->class_map_by_alloc_size[i] = predefined_class->id;
	}

	/*
	 * Based on the defined categories, a set of allocation classes is
	 * created. The unit size of those classes is depended on the category
	 * initial size and step.
	 */
	size_t granularity_mask = ALLOC_BLOCK_SIZE_GEN - 1;
	for (int c = 1; c < MAX_ALLOC_CATEGORIES; ++c) {
		size_t n = categories[c - 1].size + ALLOC_BLOCK_SIZE_GEN;
		do {
			if (alloc_class_find_or_create(ac, n) == NULL)
				goto error_alloc_class_create;

			float stepf = (float)n * categories[c].step;
			size_t stepi = (size_t)stepf;
			stepi = stepf == stepi ? stepi : stepi + 1;

			n += (stepi + (granularity_mask)) & ~granularity_mask;
		} while (n <= categories[c].size);
	}

	/*
	 * Find the largest alloc class and use it's unit size as run allocation
	 * threshold.
	 */
	uint8_t largest_aclass_slot;
	for (largest_aclass_slot = MAX_ALLOCATION_CLASSES - 1;
			largest_aclass_slot > 0 &&
			ac->aclasses[largest_aclass_slot] == NULL;
			--largest_aclass_slot) {
		/* intentional NOP */
	}

	struct alloc_class *c = ac->aclasses[largest_aclass_slot];

	/*
	 * The actual run might contain less unit blocks than the theoretical
	 * unit max variable. This may be the case for very large unit sizes.
	 */
	size_t real_unit_max = c->run.bitmap_nallocs < RUN_UNIT_MAX_ALLOC ?
		c->run.bitmap_nallocs : RUN_UNIT_MAX_ALLOC;

	size_t theoretical_run_max_size = c->unit_size * real_unit_max;

	ac->last_run_max_size = MAX_RUN_SIZE > theoretical_run_max_size ?
		theoretical_run_max_size : MAX_RUN_SIZE;

	/*
	 * Now that the alloc classes are created, the bucket with the minimal
	 * internal fragmentation for that size is chosen.
	 */
	for (size_t i = FIRST_GENERATED_CLASS_SIZE / ac->granularity;
		i <= ac->last_run_max_size / ac->granularity; ++i) {
		struct alloc_class *c = alloc_class_find_min_frag(ac,
				i * ac->granularity);

		ac->class_map_by_alloc_size[i] = c->id;
	}

#ifdef DEBUG
	/*
	 * Verify that each bucket's unit size points back to the bucket by the
	 * bucket map. This must be true for the default allocation classes,
	 * otherwise duplicate buckets will be created.
	 */
	for (size_t i = 0; i < MAX_ALLOCATION_CLASSES; ++i) {
		struct alloc_class *c = ac->aclasses[i];

		if (c != NULL) {
			ASSERTeq(i, c->id);
			uint8_t class_id = ac->class_map_by_unit_size[
				SIZE_TO_CLASS_MAP_INDEX(c->unit_size,
					ac->granularity)];
			ASSERTeq(class_id, c->id);
		}
	}
#endif

	return ac;

error_alloc_class_create:
	alloc_class_collection_delete(ac);

	return NULL;
}

/*
 * alloc_class_collection_delete -- deletes the allocation class collection and
 *	all of the classes within it
 */
void
alloc_class_collection_delete(struct alloc_class_collection *ac)
{
	for (size_t i = 0; i < MAX_ALLOCATION_CLASSES; ++i) {
		struct alloc_class *c = ac->aclasses[i];
		if (c != NULL) {
			alloc_class_delete(ac, c);
		}
	}

	Free(ac->class_map_by_alloc_size);
	Free(ac->class_map_by_unit_size);
	Free(ac);
}

/*
 * alloc_class_by_map -- (internal) returns the allocation class found for
 *	given size in the provided map
 */
static struct alloc_class *
alloc_class_by_map(struct alloc_class_collection *ac,
	uint8_t *map, size_t size)
{
	if (size < ac->last_run_max_size) {
		uint8_t class_id = map[
			SIZE_TO_CLASS_MAP_INDEX(size, ac->granularity)];

		if (class_id == MAX_ALLOCATION_CLASSES) {
			if (ac->fail_on_missing_class)
				return NULL;
			else
				return ac->aclasses[DEFAULT_ALLOC_CLASS_ID];
		}

		return ac->aclasses[class_id];
	} else {
		return ac->aclasses[DEFAULT_ALLOC_CLASS_ID];
	}
}

/*
 * alloc_class_by_alloc_size -- returns allocation class that is assigned
 *	to handle an allocation of the provided size
 */
struct alloc_class *
alloc_class_by_alloc_size(struct alloc_class_collection *ac, size_t size)
{
	return alloc_class_by_map(ac, ac->class_map_by_alloc_size, size);
}

/*
 * alloc_class_by_unit_size -- returns the allocation class that has the given
 *	unit size
 */
struct alloc_class *
alloc_class_by_unit_size(struct alloc_class_collection *ac, size_t size)
{
	return alloc_class_by_map(ac, ac->class_map_by_unit_size, size);
}

/*
 * alloc_class_by_id -- returns the allocation class with an id
 */
struct alloc_class *
alloc_class_by_id(struct alloc_class_collection *ac, uint8_t id)
{
	return ac->aclasses[id];
}

/*
 * alloc_class_calc_size_idx -- calculates how many units does the size require
 */
ssize_t
alloc_class_calc_size_idx(struct alloc_class *c, size_t size)
{
	uint32_t size_idx = CALC_SIZE_IDX(c->unit_size,
		size + header_type_to_size[c->header_type]);

	if (c->type == CLASS_RUN) {
		if (c->header_type == HEADER_NONE && size_idx != 1)
			return -1;
		else if (size_idx > RUN_UNIT_MAX)
			return -1;
	}

	return size_idx;
}

/*
 * alloc_class_reset -- removes all allocation classes and associated
 *	resources
 */
int
alloc_class_reset(struct alloc_class_collection *ac,
	size_t granularity, size_t limit, int fail_on_missing_class)
{
	size_t maps_size = (limit / granularity) + 1;
	uint8_t *class_map_by_alloc_size;
	uint8_t *class_map_by_unit_size;
	if ((class_map_by_alloc_size = Malloc(maps_size)) == NULL) {
		return -1;
	}
	if ((class_map_by_unit_size = Malloc(maps_size)) == NULL) {
		Free(class_map_by_alloc_size);
		return -1;
	}

	ac->last_run_max_size = limit;
	ac->granularity = granularity;
	for (size_t i = 0; i < MAX_ALLOCATION_CLASSES; ++i) {
		struct alloc_class *c = ac->aclasses[i];
		if (c != NULL && c->type == CLASS_RUN) {
			alloc_class_delete(ac, c);
		}
	}
	Free(ac->class_map_by_alloc_size);
	Free(ac->class_map_by_unit_size);

	ac->class_map_by_alloc_size = class_map_by_alloc_size;
	ac->class_map_by_unit_size = class_map_by_unit_size;
	memset(ac->class_map_by_alloc_size, 0xFF, maps_size);
	memset(ac->class_map_by_unit_size, 0xFF, maps_size);

	ac->fail_on_missing_class = fail_on_missing_class;

	return 0;
}

/*
 * alloc_class_range_set -- sets the allocation class for the given range in the
 *	map
 */
int
alloc_class_range_set(struct alloc_class_collection *ac,
	struct alloc_class *c, size_t start, size_t end)
{
	/* only single unit allocs are supported with minimal header */
	if (c->header_type == HEADER_NONE) {
		if (CALC_SIZE_IDX(c->unit_size, start) > 1)
			return -1;
		if (CALC_SIZE_IDX(c->unit_size, end) > 1)
			return -1;
	}

	size_t start_blocks = SIZE_TO_CLASS_MAP_INDEX(start, ac->granularity);
	size_t end_blocks = SIZE_TO_CLASS_MAP_INDEX(end, ac->granularity);

	for (size_t n = start_blocks; n <= end_blocks; ++n) {
		ac->class_map_by_alloc_size[n] = c->id;
	}

	return 0;
}

/*
 * alloc_class_granularity -- returns the allocation class map granularity
 */
size_t
alloc_class_granularity(struct alloc_class_collection *ac)
{
	return ac->granularity;
}

/*
 * alloc_class_limit -- returns the limit in bytes of the allocation classes
 *	map
 */
size_t
alloc_class_limit(struct alloc_class_collection *ac)
{
	return ac->last_run_max_size;
}

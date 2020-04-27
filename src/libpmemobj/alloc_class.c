// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016-2020, Intel Corporation */

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
#include "critnib.h"

#define RUN_CLASS_KEY_PACK(map_idx_s, flags_s, size_idx_s)\
((uint64_t)(map_idx_s) << 32 |\
(uint64_t)(flags_s) << 16 |\
(uint64_t)(size_idx_s))

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

/*
 * The first predefined allocation class size
 */
#define MIN_UNIT_SIZE 128

static const struct {
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
 * Target number of allocations per run instance.
 */
#define RUN_MIN_NALLOCS 200

/*
 * Hard limit of chunks per single run.
 */
#define RUN_SIZE_IDX_CAP (16)

#define ALLOC_CLASS_DEFAULT_FLAGS CHUNK_FLAG_FLEX_BITMAP

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
	struct critnib *class_map_by_unit_size;

	int fail_on_missing_class;
	int autogenerate_on_missing_class;
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
	LOG(10, NULL);

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
 * alloc_class_reserve -- reserve the specified class id
 */
int
alloc_class_reserve(struct alloc_class_collection *ac, uint8_t id)
{
	LOG(10, NULL);

	return util_bool_compare_and_swap64(&ac->aclasses[id],
			NULL, ACLASS_RESERVED) ? 0 : -1;
}

/*
 * alloc_class_reservation_clear -- removes the reservation on class id
 */
static void
alloc_class_reservation_clear(struct alloc_class_collection *ac, int id)
{
	LOG(10, NULL);

	int ret = util_bool_compare_and_swap64(&ac->aclasses[id],
		ACLASS_RESERVED, NULL);
	ASSERT(ret);
}

/*
 * alloc_class_new -- creates a new allocation class
 */
struct alloc_class *
alloc_class_new(int id, struct alloc_class_collection *ac,
	enum alloc_class_type type, enum header_type htype,
	size_t unit_size, size_t alignment,
	uint32_t size_idx)
{
	LOG(10, NULL);

	struct alloc_class *c = Malloc(sizeof(*c));
	if (c == NULL)
		goto error_class_alloc;

	c->unit_size = unit_size;
	c->header_type = htype;
	c->type = type;
	c->flags = (uint16_t)
		(header_type_to_flag[c->header_type] |
		(alignment ? CHUNK_FLAG_ALIGNED : 0)) |
		ALLOC_CLASS_DEFAULT_FLAGS;

	switch (type) {
		case CLASS_HUGE:
			id = DEFAULT_ALLOC_CLASS_ID;
			break;
		case CLASS_RUN:
			c->rdsc.alignment = alignment;
			memblock_run_bitmap(&size_idx, c->flags, unit_size,
				alignment, NULL, &c->rdsc.bitmap);
			c->rdsc.nallocs = c->rdsc.bitmap.nbits;
			c->rdsc.size_idx = size_idx;

			/* these two fields are duplicated from class */
			c->rdsc.unit_size = c->unit_size;
			c->rdsc.flags = c->flags;

			uint8_t slot = (uint8_t)id;
			if (id < 0 && alloc_class_find_first_free_slot(ac,
					&slot) != 0)
				goto error_class_alloc;
			id = slot;

			size_t map_idx = SIZE_TO_CLASS_MAP_INDEX(c->unit_size,
				ac->granularity);
			ASSERT(map_idx <= UINT32_MAX);
			uint32_t map_idx_s = (uint32_t)map_idx;
			uint16_t size_idx_s = (uint16_t)size_idx;
			uint16_t flags_s = (uint16_t)c->flags;
			uint64_t k = RUN_CLASS_KEY_PACK(map_idx_s,
				flags_s, size_idx_s);
			if (critnib_insert(ac->class_map_by_unit_size,
			    k, c) != 0) {
				ERR("unable to register allocation class");
				goto error_map_insert;
			}

			break;
		default:
			ASSERT(0);
	}

	c->id = (uint8_t)id;
	ac->aclasses[c->id] = c;
	return c;

error_map_insert:
	Free(c);
error_class_alloc:
	if (id >= 0)
		alloc_class_reservation_clear(ac, id);
	return NULL;
}

/*
 * alloc_class_delete -- (internal) deletes an allocation class
 */
void
alloc_class_delete(struct alloc_class_collection *ac,
	struct alloc_class *c)
{
	LOG(10, NULL);

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
	LOG(10, NULL);

	COMPILE_ERROR_ON(MAX_ALLOCATION_CLASSES > UINT8_MAX);
	uint64_t required_size_bytes = n * RUN_MIN_NALLOCS;
	uint32_t required_size_idx = 1;
	if (required_size_bytes > RUN_DEFAULT_SIZE) {
		required_size_bytes -= RUN_DEFAULT_SIZE;
		required_size_idx +=
			CALC_SIZE_IDX(CHUNKSIZE, required_size_bytes);
		if (required_size_idx > RUN_SIZE_IDX_CAP)
			required_size_idx = RUN_SIZE_IDX_CAP;
	}

	for (int i = MAX_ALLOCATION_CLASSES - 1; i >= 0; --i) {
		struct alloc_class *c = ac->aclasses[i];

		if (c == NULL || c->type == CLASS_HUGE ||
				c->rdsc.size_idx < required_size_idx)
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
	struct run_bitmap b;
	size_t runsize_bytes = 0;
	do {
		if (runsize_bytes != 0) /* don't increase on first iteration */
			n += ALLOC_BLOCK_SIZE_GEN;

		uint32_t size_idx = required_size_idx;
		memblock_run_bitmap(&size_idx, ALLOC_CLASS_DEFAULT_FLAGS, n, 0,
			NULL, &b);

		runsize_bytes = RUN_CONTENT_SIZE_BYTES(size_idx) - b.size;
	} while ((runsize_bytes % n) > MAX_RUN_WASTED_BYTES);

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

	return alloc_class_new(-1, ac, CLASS_RUN, HEADER_COMPACT, n, 0,
		required_size_idx);
}

/*
 * alloc_class_find_min_frag -- searches for an existing allocation
 * class that will provide the smallest internal fragmentation for the given
 * size.
 */
static struct alloc_class *
alloc_class_find_min_frag(struct alloc_class_collection *ac, size_t n)
{
	LOG(10, NULL);

	struct alloc_class *best_c = NULL;
	size_t lowest_waste = SIZE_MAX;

	ASSERTne(n, 0);

	/*
	 * Start from the largest buckets in order to minimize unit size of
	 * allocated memory blocks.
	 */
	for (int i = MAX_ALLOCATION_CLASSES - 1; i >= 0; --i) {
		struct alloc_class *c = ac->aclasses[i];

		/* can't use alloc classes /w no headers by default */
		if (c == NULL || c->header_type == HEADER_NONE)
			continue;

		size_t real_size = n + header_type_to_size[c->header_type];

		size_t units = CALC_SIZE_IDX(c->unit_size, real_size);

		/* can't exceed the maximum allowed run unit max */
		if (c->type == CLASS_RUN && units > RUN_UNIT_MAX_ALLOC)
			continue;

		if (c->unit_size * units == real_size)
			return c;

		size_t waste = (c->unit_size * units) - real_size;

		/*
		 * If we assume that the allocation class is only ever going to
		 * be used with exactly one size, the effective internal
		 * fragmentation would be increased by the leftover
		 * memory at the end of the run.
		 */
		if (c->type == CLASS_RUN) {
			size_t wasted_units = c->rdsc.nallocs % units;
			size_t wasted_bytes = wasted_units * c->unit_size;
			size_t waste_avg_per_unit = wasted_bytes /
				c->rdsc.nallocs;

			waste += waste_avg_per_unit;
		}

		if (best_c == NULL || lowest_waste > waste) {
			best_c = c;
			lowest_waste = waste;
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
	LOG(10, NULL);

	struct alloc_class_collection *ac = Zalloc(sizeof(*ac));
	if (ac == NULL)
		return NULL;

	ac->granularity = ALLOC_BLOCK_SIZE;
	ac->last_run_max_size = MAX_RUN_SIZE;
	ac->fail_on_missing_class = 0;
	ac->autogenerate_on_missing_class = 1;

	size_t maps_size = (MAX_RUN_SIZE / ac->granularity) + 1;

	if ((ac->class_map_by_alloc_size = Malloc(maps_size)) == NULL)
		goto error;
	if ((ac->class_map_by_unit_size = critnib_new()) == NULL)
		goto error;

	memset(ac->class_map_by_alloc_size, 0xFF, maps_size);

	if (alloc_class_new(-1, ac, CLASS_HUGE, HEADER_COMPACT,
		CHUNKSIZE, 0, 1) == NULL)
		goto error;

	struct alloc_class *predefined_class =
		alloc_class_new(-1, ac, CLASS_RUN, HEADER_COMPACT,
			MIN_UNIT_SIZE, 0, 1);
	if (predefined_class == NULL)
		goto error;

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
				goto error;

			float stepf = (float)n * categories[c].step;
			size_t stepi = (size_t)stepf;
			stepi = (stepf - (float)stepi < FLT_EPSILON) ?
				stepi : stepi + 1;

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
	size_t real_unit_max = c->rdsc.nallocs < RUN_UNIT_MAX_ALLOC ?
		c->rdsc.nallocs : RUN_UNIT_MAX_ALLOC;

	size_t theoretical_run_max_size = c->unit_size * real_unit_max;

	ac->last_run_max_size = MAX_RUN_SIZE > theoretical_run_max_size ?
		theoretical_run_max_size : MAX_RUN_SIZE;

#ifdef DEBUG
	/*
	 * Verify that each bucket's unit size points back to the bucket by the
	 * bucket map. This must be true for the default allocation classes,
	 * otherwise duplicate buckets will be created.
	 */
	for (size_t i = 0; i < MAX_ALLOCATION_CLASSES; ++i) {
		struct alloc_class *c = ac->aclasses[i];

		if (c != NULL && c->type == CLASS_RUN) {
			ASSERTeq(i, c->id);
			ASSERTeq(alloc_class_by_run(ac, c->unit_size,
				c->flags, c->rdsc.size_idx), c);
		}
	}
#endif

	return ac;

error:
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
	LOG(10, NULL);

	for (size_t i = 0; i < MAX_ALLOCATION_CLASSES; ++i) {
		struct alloc_class *c = ac->aclasses[i];
		if (c != NULL) {
			alloc_class_delete(ac, c);
		}
	}

	if (ac->class_map_by_unit_size)
		critnib_delete(ac->class_map_by_unit_size);
	Free(ac->class_map_by_alloc_size);
	Free(ac);
}

/*
 * alloc_class_assign_by_size -- (internal) chooses the allocation class that
 *	best approximates the provided size
 */
static struct alloc_class *
alloc_class_assign_by_size(struct alloc_class_collection *ac,
	size_t size)
{
	LOG(10, NULL);

	size_t class_map_index = SIZE_TO_CLASS_MAP_INDEX(size,
		ac->granularity);

	struct alloc_class *c = alloc_class_find_min_frag(ac,
		class_map_index * ac->granularity);
	ASSERTne(c, NULL);

	/*
	 * We don't lock this array because locking this section here and then
	 * bailing out if someone else was faster would be still slower than
	 * just calculating the class and failing to assign the variable.
	 * We are using a compare and swap so that helgrind/drd don't complain.
	 */
	util_bool_compare_and_swap64(
		&ac->class_map_by_alloc_size[class_map_index],
		MAX_ALLOCATION_CLASSES, c->id);

	return c;
}

/*
 * alloc_class_by_alloc_size -- returns allocation class that is assigned
 *	to handle an allocation of the provided size
 */
struct alloc_class *
alloc_class_by_alloc_size(struct alloc_class_collection *ac, size_t size)
{
	if (size < ac->last_run_max_size) {
		uint8_t class_id = ac->class_map_by_alloc_size[
			SIZE_TO_CLASS_MAP_INDEX(size, ac->granularity)];

		if (class_id == MAX_ALLOCATION_CLASSES) {
			if (ac->fail_on_missing_class)
				return NULL;
			else if (ac->autogenerate_on_missing_class)
				return alloc_class_assign_by_size(ac, size);
			else
				return ac->aclasses[DEFAULT_ALLOC_CLASS_ID];
		}

		return ac->aclasses[class_id];
	} else {
		return ac->aclasses[DEFAULT_ALLOC_CLASS_ID];
	}
}

/*
 * alloc_class_by_run -- returns the allocation class that has the given
 *	unit size
 */
struct alloc_class *
alloc_class_by_run(struct alloc_class_collection *ac,
	size_t unit_size, uint16_t flags, uint32_t size_idx)
{
	size_t map_idx = SIZE_TO_CLASS_MAP_INDEX(unit_size, ac->granularity);
	ASSERT(map_idx <= UINT32_MAX);
	uint32_t map_idx_s = (uint32_t)map_idx;
	ASSERT(size_idx <= UINT16_MAX);
	uint16_t size_idx_s = (uint16_t)size_idx;
	uint16_t flags_s = (uint16_t)flags;

	return critnib_get(ac->class_map_by_unit_size,
		RUN_CLASS_KEY_PACK(map_idx_s, flags_s, size_idx_s));
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
		else if (size_idx > c->rdsc.nallocs)
			return -1;
	}

	return size_idx;
}

/*
 * Copyright 2019, Intel Corporation
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

#include <inttypes.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <ex_common.h>
#include <sys/stat.h>
#include <libpmemobj.h>

POBJ_LAYOUT_BEGIN(colony);
POBJ_LAYOUT_TOID(colony, struct colony);
POBJ_LAYOUT_END(colony);

POBJ_LAYOUT_BEGIN(block);
POBJ_LAYOUT_TOID(block, struct block);
POBJ_LAYOUT_END(block);

POBJ_LAYOUT_BEGIN(free_block);
POBJ_LAYOUT_TOID(free_block, struct free_block);
POBJ_LAYOUT_END(free_block);

POBJ_LAYOUT_BEGIN(free_idx);
POBJ_LAYOUT_TOID(free_idx, struct free_idx);
POBJ_LAYOUT_END(free_idx);

POBJ_LAYOUT_BEGIN(elements);
POBJ_LAYOUT_TOID(elements, struct elements);
POBJ_LAYOUT_END(elements);

POBJ_LAYOUT_BEGIN(items);
POBJ_LAYOUT_TOID(items, int);
POBJ_LAYOUT_TOID(items, PMEMoid);
POBJ_LAYOUT_END(items);

POBJ_LAYOUT_BEGIN(jumps);
POBJ_LAYOUT_TOID(jumps, size_t);
POBJ_LAYOUT_END(jumps);

#define MAX_BUFFLEN 30

enum array_types {
	UNKNOWN_ARRAY_TYPE,
	INT_ARRAY_TYPE,
	PMEMOID_ARRAY_TYPE,
	MAX_ARRAY_TYPE
};

struct elements {
	/* indicates the beginning of table of items */
	PMEMoid items;
	/* indicates the beginning of table of numbers of elements to skip */
	PMEMoid jumps;
};

struct block {
	/* pointer to table of elements */
	TOID(struct elements) table;

	/* number of block in colony */
	size_t block_nr;
	/* index of last added element (idx in block) */
	size_t idx_last;
	/* number of free indexes in block */
	size_t free_idx_count_block;

	/* pointer to the previous block */
	TOID(struct block) prev;
	/* pointer to the next block */
	TOID(struct block) next;
};

struct free_idx {
	/* index (in colony) of free element (hole) */
	size_t idx_free;

	/* pointer to the previous free element */
	TOID(struct free_idx) prev;
	/* pointer to the next free element */
	TOID(struct free_idx) next;
};

struct free_block {
	/* pointer to the free block (block of holes) */
	TOID(struct block) block_free;

	/* pointer to the previous free block */
	TOID(struct free_block) prev;
	/* pointer to the next free block */
	TOID(struct free_block) next;
};

struct colony {
	char name[MAX_BUFFLEN];

	/* type of elements in colony */
	enum array_types element_type;
	/* number of occupied elements */
	size_t colony_size;
	/* total capacity = block_capacity * block_count */
	size_t colony_capacity;

	/* number of elements in each block */
	size_t block_capacity;
	/* number of blocks in colony */
	size_t block_count;
	/* pointer to the first block */
	TOID(struct block) block_head;
	/* pointer to the last block */
	TOID(struct block) block_tail;


	/* number of free elements (holes) */
	size_t free_idx_count;
	/* pointer to the last free element */
	TOID(struct free_idx) free_idx_tail;

	/* number of free blocks (blocks od holes) */
	size_t free_block_count;
	/* pointer to the last free block */
	TOID(struct free_block) free_block_tail;
};

/*
 * colony_int_print -- prints the colony if the type of elements is int
 */
static void
colony_int_print(TOID(struct colony) c)
{
	TOID(struct block) b = D_RO(c)->block_head;
	TOID(size_t) jumps;
	TOID(int) items;

	for (size_t i = 0; i < D_RO(c)->block_count; i++) {
		TOID_ASSIGN(jumps, D_RO(D_RO(b)->table)->jumps);
		TOID_ASSIGN(items, D_RO(D_RO(b)->table)->items);

		for (size_t j = D_RO(jumps)[0]; j < D_RO(c)->block_capacity;
									j++) {
			if (D_RO(jumps)[j] != 0)
				continue;
			size_t col_idx = i * D_RO(c)->block_capacity + j;
			printf("col_idx: %zu\t\titem: %d\n", col_idx,
								D_RO(items)[j]);
			j += D_RO(jumps)[j];
		}
		b = D_RO(b)->next;
	}
}

/*
 * get type -- parses argument given as type of colony
 */
static enum array_types
get_type(const char *type_name)
{
	const char *names[MAX_ARRAY_TYPE] = { "", "int", "pmemoid" };
	enum array_types type;

	for (type = (enum array_types)(MAX_ARRAY_TYPE - 1);
		type > UNKNOWN_ARRAY_TYPE;
		type = (enum array_types)(type - 1)) {
		if (strcmp(names[type], type_name) == 0)
			break;
	}
	if (type == UNKNOWN_ARRAY_TYPE)
		fprintf(stderr, "\nunknown type: %s\n", type_name);

	return type;
}

/*
 * capacity_get -- returns the capacity of colony
 */
static size_t
capacity_get(TOID(struct colony) c)
{
	return D_RO(c)->colony_capacity;
}

/*
 * size_get -- returns the number of occupied addresses
 */
static size_t
size_get(TOID(struct colony) c)
{
	return D_RO(c)->colony_size;
}

/*
 * block_get_by_idx -- returns the pointer to the block in which element with
 * given index resides
 */
static TOID(struct block)
block_get_by_idx(TOID(struct colony) c, size_t colony_idx)
{
	TOID(struct block) block_with_idx = D_RO(c)->block_head;

	size_t block_nr = colony_idx / D_RO(c)->block_capacity;
	for (size_t i = 0; i < block_nr; i++)
		block_with_idx = D_RO(block_with_idx)->next;

	return block_with_idx;
}

/*
 * block_get_by_nr -- returns the pointer to the block with given block_nr
 */
static TOID(struct block)
block_get_by_nr(TOID(struct colony) c, size_t block_nr)
{
	TOID(struct block) block_with_nr = D_RO(c)->block_head;

	for (size_t i = 0; i < block_nr; i++)
		block_with_nr = D_RO(block_with_nr)->next;

	return block_with_nr;
}


/*
 * skipfield_to_unskipped -- sets values in the skiplist after insertion of the
 * item
 */
static void
skipfield_to_unskipped(TOID(struct colony) c, TOID(struct block) block_with_idx,
							size_t block_idx)
{
	assert(pmemobj_tx_stage() == TX_STAGE_WORK);

	TOID(size_t) jumps;
	TOID_ASSIGN(jumps, D_RO(D_RO(block_with_idx)->table)->jumps);

	pmemobj_tx_add_range_direct(D_RO(jumps), sizeof(size_t) * D_RO(c)->
								block_capacity);
	if (block_idx == 0 && D_RO(jumps)[block_idx + 1] == 0) {
		D_RW(jumps)[block_idx] = 0;
	} else if (block_idx == 0 && D_RO(jumps)[block_idx + 1] != 0) {
		size_t x = D_RO(jumps)[block_idx] - 1;
		D_RW(jumps)[block_idx] = 0;
		D_RW(jumps)[block_idx + 1] = x;
		x--;
		size_t j = 2;
		while (x != 0) {
			if (block_idx + j < D_RO(c)->block_capacity)
				D_RW(jumps)[block_idx + j] = j;
			else break;
			j++;
			x--;
		}
	} else if (block_idx == D_RO(c)->block_capacity - 1 &&
					D_RO(jumps)[block_idx - 1] == 0) {
		D_RW(jumps)[block_idx] = 0;
	} else if (block_idx == D_RO(c)->block_capacity - 1 &&
					D_RO(jumps)[block_idx - 1] != 0) {
		size_t x = D_RO(jumps)[block_idx] - 1;
		D_RW(jumps)[block_idx - x] = x;
		D_RW(jumps)[block_idx] = 0;
	} else if (D_RO(jumps)[block_idx - 1] == 0 && D_RO(jumps)[block_idx + 1]
									== 0) {
		D_RW(jumps)[block_idx] = 0;
	} else if (D_RO(jumps)[block_idx - 1] != 0 && D_RO(jumps)[block_idx + 1]
									== 0) {
		size_t x = D_RO(jumps)[block_idx] - 1;
		D_RW(jumps)[block_idx - x] = x;
		D_RW(jumps)[block_idx] = 0;
	} else if (D_RO(jumps)[block_idx - 1] == 0 && D_RO(jumps)[block_idx + 1]
									!= 0) {
		size_t x = D_RO(jumps)[block_idx] - 1;
		D_RW(jumps)[block_idx] = 0;
		D_RW(jumps)[block_idx + 1] = x;
		x--;
		size_t j = 2;
		while (x != 0) {
			if (block_idx + j < D_RO(c)->block_capacity)
				D_RW(jumps)[block_idx + j] = j;
			else break;
			j++;
			x--;
		}
	} else if (D_RO(jumps)[block_idx - 1] != 0 && D_RO(jumps)[block_idx + 1]
									!= 0) {
		size_t y = D_RO(jumps)[block_idx];
		size_t z = block_idx - (y - 1);
		size_t x = D_RO(jumps)[z] - y;
		D_RW(jumps)[block_idx + 1] = x;

		D_RW(jumps)[z] = y - 1;
		x--;
		D_RW(jumps)[block_idx] = 0;
		size_t j = 2;
		while (x != 0) {
			if (block_idx + j < D_RO(c)->block_capacity)
				D_RW(jumps)[block_idx + j] = j;
			else break;
			j++;
			x--;
		}
	}
}

/*
 * int_insert_at_idx -- inserts the integer item into the colony at colony_idx
 */
static void
int_insert_at_idx(PMEMobjpool *pop, TOID(struct colony) c, size_t colony_idx,
								int item)
{
	assert(pmemobj_tx_stage() == TX_STAGE_WORK);

	TOID(struct block) block_with_free_el = block_get_by_idx(c, colony_idx);
	TOID(int) items;
	TOID_ASSIGN(items, D_RO(D_RO(block_with_free_el)->table)->items);

	size_t block_idx = colony_idx % D_RO(c)->block_capacity;
	pmemobj_tx_add_range_direct(&D_RW(items)[block_idx], sizeof(int));
	D_RW(items)[block_idx] = item;
	skipfield_to_unskipped(c, block_with_free_el, block_idx);
}

/*
 * pmemoid_insert_at_idx -- inserts the PMEMoid item in the colony at
 * colony_idx
 */
static void
pmemoid_insert_at_idx(PMEMobjpool *pop, TOID(struct colony) c,
						size_t colony_idx, PMEMoid item)
{
	assert(pmemobj_tx_stage() == TX_STAGE_WORK);

	TOID(struct block) block_with_free_el = block_get_by_idx(c, colony_idx);
	TOID(PMEMoid) items;
	TOID_ASSIGN(items, D_RO(D_RO(block_with_free_el)->table)->items);

	size_t block_idx = colony_idx % D_RO(c)->block_capacity;
	pmemobj_tx_add_range_direct(&D_RO(items)[block_idx], sizeof(PMEMoid));
	D_RW(items)[block_idx].pool_uuid_lo = item.pool_uuid_lo;
	D_RW(items)[block_idx].off = item.off;
	skipfield_to_unskipped(c, block_with_free_el, block_idx);
}

/*
 * insert_at_idx -- calls a function suitable for the type of colony and item
 * that is inserted into the colony at colony_idx
 */
static void
insert_at_idx(PMEMobjpool *pop, TOID(struct colony) c, size_t colony_idx,
								void *item)
{
	assert(pmemobj_tx_stage() == TX_STAGE_WORK);

	if (D_RO(c)->element_type == INT_ARRAY_TYPE)
		int_insert_at_idx(pop, c, colony_idx, *(int *)item);
	else if (D_RO(c)->element_type == PMEMOID_ARRAY_TYPE)
		pmemoid_insert_at_idx(pop, c, colony_idx, *(PMEMoid *)item);
}

/*
 * skipfield_to_skipped -- sets values in the skiplist after removal of the item
 */
static void
skipfield_to_skipped(TOID(struct colony) c, TOID(struct block) block_with_idx,
							size_t block_idx) {
	assert(pmemobj_tx_stage() == TX_STAGE_WORK);

	TOID(size_t) jumps;
	TOID_ASSIGN(jumps, D_RO(D_RO(block_with_idx)->table)->jumps);

	pmemobj_tx_add_range_direct(D_RO(jumps), sizeof(size_t) * D_RO(c)->
								block_capacity);
	if (block_idx == 0 && D_RO(jumps)[block_idx + 1] == 0) {
		D_RW(jumps)[block_idx] = 1;
	} else if ((block_idx == 0 && D_RO(jumps)[block_idx + 1] != 0)) {
		size_t x = D_RO(jumps)[block_idx + 1];
		D_RW(jumps)[block_idx] = x + 1;
		size_t j = 1;
		while (x != 0) {
			if (block_idx + j < D_RO(c)->block_capacity)
				D_RW(jumps)[block_idx + j] = j + 1;
			else
				break;
			j++;
			x--;
		}
	} else if (block_idx == D_RO(c)->block_capacity - 1 &&
					D_RO(jumps)[block_idx - 1] == 0) {
		D_RW(jumps)[block_idx] = 1;
	} else if (block_idx == D_RO(c)->block_capacity - 1 &&
					D_RO(jumps)[block_idx - 1] != 0) {
		D_RW(jumps)[block_idx] = 1 + D_RO(jumps)[block_idx - 1];
		size_t y = block_idx - D_RO(jumps)[block_idx - 1];
		D_RW(jumps)[y] = D_RO(jumps)[block_idx];
	} else if (D_RO(jumps)[block_idx - 1] == 0 && D_RO(jumps)[block_idx + 1]
									== 0) {
		D_RW(jumps)[block_idx] = 1;
	} else if (D_RO(jumps)[block_idx - 1] != 0 && D_RO(jumps)[block_idx + 1]
									== 0) {
		D_RW(jumps)[block_idx] = 1 + D_RO(jumps)[block_idx - 1];
		size_t y = block_idx - D_RO(jumps)[block_idx - 1];
		D_RW(jumps)[y] = D_RO(jumps)[block_idx];
	} else if (D_RO(jumps)[block_idx - 1] == 0 && D_RO(jumps)[block_idx + 1]
									!= 0) {
		size_t x = D_RO(jumps)[block_idx + 1];
		D_RW(jumps)[block_idx] = x + 1;
		size_t j = 1;
		while (x != 0) {
			if (block_idx + j < D_RO(c)->block_capacity)
				D_RW(jumps)[block_idx + j] = j + 1;
			else break;
			j++;
			x--;
		}
	} else if (D_RO(jumps)[block_idx - 1] != 0 && D_RO(jumps)[block_idx + 1]
									!= 0) {
		size_t x = 1 + D_RO(jumps)[block_idx + 1];
		size_t y = D_RO(jumps)[block_idx - 1];
		D_RW(jumps)[block_idx - y] = D_RO(jumps)[block_idx - y] + x;
		y++;
		size_t j = block_idx;
		while (x != 0) {
			if (j < D_RO(c)->block_capacity)
				D_RW(jumps)[j] = y;
			else break;
			j++;
			y++;
			x--;
		}
	}
}

/*
 * table_int_create -- allocates the structure object containing table of
 * integers and associated table of jumps in the block
 */
static void
table_int_create(PMEMobjpool *pop, TOID(struct colony) c, TOID(struct block) *b)
{
	size_t s1 = sizeof(int) * D_RO(c)->block_capacity;
	size_t s2 = sizeof(size_t) * D_RO(c)->block_capacity;

	TX_BEGIN(pop) {
		TX_ADD_FIELD(*b, table);

		D_RW(*b)->table = TX_ZNEW(struct elements);
		D_RW(D_RW(*b)->table)->items = pmemobj_tx_zalloc(s1,
							TOID_TYPE_NUM(int));
		D_RW(D_RW(*b)->table)->jumps = pmemobj_tx_alloc(s2,
							TOID_TYPE_NUM(size_t));
		TOID(size_t) jumps;
		TOID_ASSIGN(jumps, D_RO(D_RO(*b)->table)->jumps);

		D_RW(jumps)[0] = D_RO(c)->block_capacity;
		for (size_t i = 1; i < D_RO(c)->block_capacity; i++)
			D_RW(jumps)[i] = i + 1;
	} TX_ONABORT {
		fprintf(stderr, "%s: transaction aborted: %s\n", __func__,
							pmemobj_errormsg());
		abort();
	} TX_END
}

/*
 * table_pmemoid_create -- allocates the structure object containing table of
 * PMEMoids and associated table of jumps in the block
 */
static void
table_pmemoid_create(PMEMobjpool *pop, TOID(struct colony) c,
							TOID(struct block) *b)
{
	size_t s1 = sizeof(PMEMoid) * (D_RO(c)->block_capacity);
	size_t s2 = sizeof(size_t) * D_RO(c)->block_capacity;

	TX_BEGIN(pop) {
		TX_ADD_FIELD(*b, table);

		D_RW(*b)->table = TX_ZNEW(struct elements);
		D_RW(D_RW(*b)->table)->items = pmemobj_tx_zalloc(s1,
							TOID_TYPE_NUM(PMEMoid));
		D_RW(D_RW(*b)->table)->jumps = pmemobj_tx_alloc(s2,
							TOID_TYPE_NUM(size_t));
		TOID(size_t) jumps;
		TOID_ASSIGN(jumps, D_RO(D_RO(*b)->table)->jumps);

		D_RW(jumps)[0] = D_RO(c)->block_capacity;
		for (size_t i = 1; i < D_RO(c)->block_capacity; i++)
			D_RW(jumps)[i] = i + 1;
	} TX_ONABORT {
		fprintf(stderr, "%s: transaction aborted: %s\n", __func__,
							pmemobj_errormsg());
		abort();
	} TX_END
}

/*
 * table_delete -- frees the table of items, table of jumps, pointer to the
 * structure object
 */
static void
table_delete(PMEMobjpool *pop, TOID(struct elements) table_del)
{
	TX_BEGIN(pop) {
		pmemobj_tx_free(D_RO(table_del)->items);
		pmemobj_tx_free(D_RO(table_del)->jumps);
		TX_FREE(table_del);
	} TX_ONABORT {
		fprintf(stderr, "%s: transaction aborted: %s\n", __func__,
							pmemobj_errormsg());
		abort();
	} TX_END
}

/*
 * block_init -- allocates a block, creates a table (the structure object
 * containing two tables), assigns initial values
 */
static void
block_init(PMEMobjpool *pop, TOID(struct colony) c, TOID(struct block) *b)
{
	assert(pmemobj_tx_stage() == TX_STAGE_WORK);

	pmemobj_tx_add_range_direct(b, sizeof(TOID(struct block)));
	*b = TX_ZNEW(struct block);

	if (D_RO(c)->element_type == INT_ARRAY_TYPE)
		table_int_create(pop, c, b);
	else if (D_RO(c)->element_type == PMEMOID_ARRAY_TYPE)
		table_pmemoid_create(pop, c, b);
	D_RW(*b)->idx_last = -1;
	D_RW(*b)->block_nr = D_RO(c)->block_count;
	D_RW(*b)->free_idx_count_block = 0;
}

/*
 * block_constructor -- constructor of the one block
 */
static void
block_constructor(PMEMobjpool *pop, TOID(struct colony) c)
{
	assert(pmemobj_tx_stage() == TX_STAGE_WORK);
	/* if it is a first block */
	if (TOID_IS_NULL(D_RO(c)->block_head)) {
		block_init(pop, c, &D_RW(c)->block_tail);
		D_RW(c)->block_head = D_RO(c)->block_tail;
		D_RW(D_RW(c)->block_tail)->prev = TOID_NULL(struct block);
		D_RW(D_RW(c)->block_tail)->next = TOID_NULL(struct block);
		D_RW(D_RW(c)->block_head)->prev = TOID_NULL(struct block);
		D_RW(D_RW(c)->block_head)->next = TOID_NULL(struct block);
	} else {
		assert(TOID_IS_NULL(D_RO(D_RO(c)->block_tail)->next));
		block_init(pop, c, &D_RW(D_RW(c)->block_tail)->next);
		D_RW(D_RW(D_RW(c)->block_tail)->next)->prev = D_RO(c)->
								block_tail;
		D_RW(c)->block_tail = D_RO(D_RO(c)->block_tail)->next;
		D_RW(D_RW(c)->block_tail)->next = TOID_NULL(struct block);
	}
	D_RW(c)->colony_capacity += D_RO(c)->block_capacity;
	D_RW(c)->block_count++;
}


/*
 * blocks_delete -- destructor of all blocks
 */
static void
blocks_delete(PMEMobjpool *pop, TOID(struct colony) *c)
{
	TOID(struct block) block_del = D_RO(*c)->block_head;

	TX_BEGIN(pop) {
		TX_ADD(*c);

		if (!TOID_EQUALS(D_RO(*c)->block_tail, D_RO(*c)->block_head)) {
			while (!TOID_EQUALS(block_del, D_RO(D_RO(*c)->
							block_tail)->prev)) {
				table_delete(pop, D_RO(block_del)->table);
				block_del = D_RO(block_del)->next;
			}
		}
		table_delete(pop, D_RO(D_RO(*c)->block_tail)->table);

		D_RW(*c)->block_head = TOID_NULL(struct block);
		D_RW(*c)->block_tail = TOID_NULL(struct block);
		D_RW(*c)->colony_capacity -= D_RO(*c)->block_capacity * D_RO(*c)
								->block_count;
		D_RW(*c)->block_count = 0;
		D_RW(*c)->colony_size = 0;
	} TX_ONABORT {
		fprintf(stderr, "%s: transaction aborted: %s\n", __func__,
							pmemobj_errormsg());
		abort();
	} TX_END
}

/*
 * unoccupied_idx_get -- returns first unoccupied (never used before) index in
 * colony (!= hole)
 */
static size_t
unoccupied_idx_get(TOID(struct colony) c)
{
	size_t block_idx = D_RO(D_RO(c)->block_tail)->idx_last + 1;
	size_t colony_idx = (D_RO(c)->block_count - 1) * D_RO(c)->block_capacity
								+ block_idx;

	return colony_idx;
}

/*
 * free_idxes_update_after_freeness_whole_block -- updates list of free
 * indexes (holes) in the block with given block_nr; when the whole block was
 * freed
 */
static void
free_idxes_update_after_freeness_whole_block(PMEMobjpool *pop,
					TOID(struct colony) c, size_t block_nr)
{
	size_t first_idx_in_block = block_nr * D_RO(c)->block_capacity;
	size_t last_idx_in_block = first_idx_in_block + D_RO(c)->block_capacity
									- 1;
	TOID(struct free_idx) idx = D_RO(c)->free_idx_tail;

	while (!TOID_IS_NULL(D_RO(idx)->prev)) {
		/* if the element had an index from this block */
		if (D_RO(idx)->idx_free >= first_idx_in_block && D_RO(idx)->
						idx_free <= last_idx_in_block) {
			/* if it was the last element of the list */
			if (TOID_EQUALS(idx, D_RO(c)->free_idx_tail)) {
				pmemobj_tx_add_range_direct(&D_RO(c)->
								free_idx_tail,
						sizeof(TOID(struct free_idx)));
				D_RW(c)->free_idx_tail = D_RO(idx)->prev;
				idx = D_RO(idx)->prev;
				TX_FREE(D_RO(idx)->next);
				pmemobj_tx_add_range_direct(&D_RO(idx)->next,
						sizeof(TOID(struct free_idx)));
				D_RW(idx)->next = TOID_NULL(struct free_idx);
				continue;
			} else {
				pmemobj_tx_add_range_direct(&D_RO(D_RO(idx)->
								prev)->next,
						sizeof(TOID(struct free_idx)));
				D_RW(D_RW(idx)->prev)->next = D_RO(idx)->next;
				pmemobj_tx_add_range_direct(&D_RO(D_RO(idx)->
								next)->prev,
						sizeof(TOID(struct free_idx)));
				D_RW(D_RW(idx)->next)->prev = D_RO(idx)->prev;
				idx = D_RO(idx)->prev;
				TX_FREE(D_RO(idx)->next);
				continue;
			}
		}
		idx = D_RO(idx)->prev;
	}
	/* if it was the first element and the index belongs to this block */
	if (D_RO(idx)->idx_free >= first_idx_in_block && D_RO(idx)->idx_free <=
							last_idx_in_block) {
		if (!TOID_IS_NULL(D_RO(idx)->next)) {
			pmemobj_tx_add_range_direct(&D_RO(D_RO(idx)->next)->
					prev, sizeof(TOID(struct free_idx)));
			D_RW(D_RW(idx)->next)->prev =
						TOID_NULL(struct free_idx);
			TX_FREE(idx);
		} else {
			idx = TOID_NULL(struct free_idx);
		}
	}
}

/*
 * free_idxes_update_after_removal_free_block -- updates indexes in the list of
 * free addresses (holes) after removal of the free block
 */
static void
free_idxes_update_after_removal_free_block(TOID(struct colony) c,
							size_t deleted_block_nr)
{
	assert(pmemobj_tx_stage() == TX_STAGE_WORK);
	/* index above which indexes should be updated */
	size_t idx_above_which = (deleted_block_nr + 1) * D_RO(c)->
								block_capacity;
	TOID(struct free_idx) free_idx_to_update = D_RO(c)->free_idx_tail;

	for (size_t i = 0; i < D_RO(c)->free_idx_count; i++) {
		if (D_RO(free_idx_to_update)->idx_free >= idx_above_which) {
			pmemobj_tx_add_range_direct(&D_RO(free_idx_to_update)->
						idx_free, sizeof(size_t));
			D_RW(free_idx_to_update)->idx_free -= D_RO(c)->
								block_capacity;
		}
		free_idx_to_update = D_RO(free_idx_to_update)->prev;
	}
}

/*
 * free_blocks_constructor -- constructor of the element in the list of free
 * blocks (all elements of the block are holes)
 */
static void
free_blocks_constructor(PMEMobjpool *pop, TOID(struct free_block) *b)
{
	assert(pmemobj_tx_stage() == TX_STAGE_WORK);
	/* if it is a first free block */
	if (TOID_IS_NULL(*b)) {
		pmemobj_tx_add_range_direct(b, sizeof(TOID(struct free_block)));
		*b = TX_ZNEW(struct free_block);

		pmemobj_tx_add_range_direct(&D_RO(*b)->prev,
					sizeof(TOID(struct free_block)));
		D_RW(*b)->prev = TOID_NULL(struct free_block);
		pmemobj_tx_add_range_direct(&D_RO(*b)->next,
					sizeof(TOID(struct free_block)));
		D_RW(*b)->next = TOID_NULL(struct free_block);
	} else {
		assert(TOID_IS_NULL(D_RO(*b)->next));

		pmemobj_tx_add_range_direct(&D_RO(*b)->next,
					sizeof(TOID(struct free_block)));
		D_RW(*b)->next = TX_ZNEW(struct free_block);
		pmemobj_tx_add_range_direct(&D_RO(D_RO(*b)->next)->prev,
					sizeof(TOID(struct free_block)));
		D_RW(D_RW(*b)->next)->prev = *b;
		pmemobj_tx_add_range_direct(b, sizeof(TOID(struct free_block)));
		*b = D_RW(*b)->next;
		pmemobj_tx_add_range_direct(&D_RO(*b)->next,
					sizeof(TOID(struct free_block)));
		D_RW(*b)->next = TOID_NULL(struct free_block);
	}
}

/*
 * free_idxes_constructor -- constructor of the element in the list of free
 * idxes (hole)
 */
static void
free_idxes_constructor(PMEMobjpool *pop, TOID(struct free_idx) *i)
{
	assert(pmemobj_tx_stage() == TX_STAGE_WORK);
	/* if it is a first free index */
	if (TOID_IS_NULL(*i)) {
		pmemobj_tx_add_range_direct(i, sizeof(TOID(struct free_idx)));
		*i = TX_ZNEW(struct free_idx);
		pmemobj_tx_add_range_direct(&D_RO(*i)->prev,
						sizeof(TOID(struct free_idx)));
		D_RW(*i)->prev = TOID_NULL(struct free_idx);
		pmemobj_tx_add_range_direct(&D_RO(*i)->next,
						sizeof(TOID(struct free_idx)));
		D_RW(*i)->next = TOID_NULL(struct free_idx);
	} else {
		pmemobj_tx_add_range_direct(&D_RO(*i)->next,
						sizeof(TOID(struct free_idx)));
		D_RW(*i)->next = TX_ZNEW(struct free_idx);
		pmemobj_tx_add_range_direct(&D_RO(D_RO(*i)->next)->prev,
						sizeof(TOID(struct free_idx)));
		D_RW(D_RW(*i)->next)->prev = *i;
		*i = D_RW(*i)->next;
		pmemobj_tx_add_range_direct(&D_RO(*i)->next,
						sizeof(TOID(struct free_idx)));
		D_RW(*i)->next = TOID_NULL(struct free_idx);
	}
}

/*
 * free_block_add_to -- adds the free block (all elements of the block are
 * holes) on the end of the list of free blocks
 */
static void
free_block_add_to(PMEMobjpool *pop, TOID(struct colony) c, size_t block_nr)
{
	assert(pmemobj_tx_stage() == TX_STAGE_WORK);

	free_blocks_constructor(pop, &D_RW(c)->free_block_tail);

	TOID(struct block) block_with_free = block_get_by_nr(c, block_nr);
	pmemobj_tx_add_range_direct(&D_RO(D_RO(c)->free_block_tail)->block_free,
						sizeof(TOID(struct block)));
	D_RW(D_RW(c)->free_block_tail)->block_free = block_with_free;

	free_idxes_update_after_freeness_whole_block(pop, c, block_nr);

	pmemobj_tx_add_range_direct(&D_RO(c)->free_block_count, sizeof(size_t));
	D_RW(c)->free_block_count++;
	pmemobj_tx_add_range_direct(&D_RO(c)->free_idx_count, sizeof(size_t));
	D_RW(c)->free_idx_count -= D_RO(c)->block_capacity - 1;
	pmemobj_tx_add_range_direct(&D_RO(block_with_free)->
					free_idx_count_block, sizeof(size_t));
	D_RW(block_with_free)->free_idx_count_block = 0;
}

/*
 * free_idx_add_to -- adds the free index (hole) on the end of the list of
 * free indexes; if all elements in block are removed moves the block to the
 * list of free blocks
 */
static void
free_idx_add_to(PMEMobjpool *pop, TOID(struct colony) c, size_t idx)
{
	assert(pmemobj_tx_stage() == TX_STAGE_WORK);

	TOID(struct block) block_with_idx = block_get_by_idx(c, idx);

	size_t block_idx = idx % D_RO(c)->block_capacity;
	pmemobj_tx_add_range_direct(&D_RO(block_with_idx)->free_idx_count_block,
								sizeof(size_t));
	D_RW(block_with_idx)->free_idx_count_block++;

	if (D_RO(block_with_idx)->free_idx_count_block == D_RO(c)->
							block_capacity) {
		size_t block_nr = idx / D_RO(c)->block_capacity;
		free_block_add_to(pop, c, block_nr);
	} else {
		free_idxes_constructor(pop, &D_RW(c)->free_idx_tail);
		pmemobj_tx_add_range_direct(&D_RO(D_RO(c)->free_idx_tail)->
						idx_free, sizeof(size_t));
		D_RW(D_RW(c)->free_idx_tail)->idx_free = idx;
		D_RW(c)->free_idx_count++;
	}
	skipfield_to_skipped(c, block_with_idx, block_idx);
}

/*
 * free_idx_add_to_rest -- adds the rest (after taking a free indesx from a free
 * block) of free indexes (holes) on the end of the list of free indexes
 */
static void
free_idx_add_to_rest(PMEMobjpool *pop, TOID(struct colony) c, size_t idx)
{
	assert(pmemobj_tx_stage() == TX_STAGE_WORK);

	TOID(struct block) block_with_idx = block_get_by_idx(c, idx);
	size_t block_idx = idx % D_RO(c)->block_capacity;
	pmemobj_tx_add_range_direct(&D_RO(block_with_idx)->free_idx_count_block,
								sizeof(size_t));
	D_RW(block_with_idx)->free_idx_count_block = D_RO(c)->block_capacity -
									1;

	for (size_t i = 1; i < D_RO(c)->block_capacity; i++) {
		free_idxes_constructor(pop, &D_RW(c)->free_idx_tail);
		pmemobj_tx_add_range_direct(&D_RO(D_RO(c)->free_idx_tail)->
						idx_free, sizeof(size_t));
		D_RW(D_RW(c)->free_idx_tail)->idx_free = idx + i;
		skipfield_to_skipped(c, block_with_idx, block_idx + i);
	}
}

/*
 * free_block_take_from -- takes the index from the last added block to the list
 * of free blocks to insertion (returns first index from the block, the others
 * adds at the end of the list of free addresses)
 */
static size_t
free_block_take_from(PMEMobjpool *pop, TOID(struct colony) c)
{
	assert(pmemobj_tx_stage() == TX_STAGE_WORK);
	/* takes the first address from the last block from the list */
	size_t idx = (D_RO(D_RO(D_RO(c)->free_block_tail)->block_free)->
					block_nr) * D_RO(c)->block_capacity;

	TOID(size_t) jumps;
	TOID_ASSIGN(jumps, D_RO(D_RO(D_RO(D_RO(c)->free_block_tail)->block_free)
							->table)->jumps);
	pmemobj_tx_add_range_direct(D_RO(jumps), sizeof(size_t) * D_RO(c)->
								block_capacity);
	for (int i = 0; i < D_RO(c)->block_capacity; i++)
		D_RW(jumps)[i] = 0;
	/* adds the rest of elements to the list of free addresses */
	free_idx_add_to_rest(pop, c, idx);
	if (!TOID_IS_NULL(D_RO(D_RO(c)->free_block_tail)->prev)) {
		pmemobj_tx_add_range_direct(&D_RO(c)->free_block_tail,
					sizeof(TOID(struct free_block)));
		D_RW(c)->free_block_tail = D_RO(D_RO(c)->free_block_tail)->prev;
		pmemobj_tx_add_range_direct(&D_RO(D_RO(c)->free_block_tail)->
					next, sizeof(TOID(struct free_block)));
		D_RW(D_RW(c)->free_block_tail)->next =
						TOID_NULL(struct free_block);
	} else {
		pmemobj_tx_add_range_direct(&D_RO(c)->free_block_tail,
					sizeof(TOID(struct free_block)));
		D_RW(c)->free_block_tail = TOID_NULL(struct free_block);
	}
	pmemobj_tx_add_range_direct(&D_RO(c)->free_block_count, sizeof(size_t));
	D_RW(c)->free_block_count--;
	pmemobj_tx_add_range_direct(&D_RO(c)->free_idx_count, sizeof(size_t));
	D_RW(c)->free_idx_count += D_RO(c)->block_capacity - 1;

	return idx;
}

/*
 * free_idx_take_from -- takes the index from the end of the list of free
 * addresses (holes) to insertion
 */
static size_t
free_idx_take_from(PMEMobjpool *pop, TOID(struct colony) c)
{
	assert(pmemobj_tx_stage() == TX_STAGE_WORK);

	/* takes the last index from the list of */
	size_t idx = D_RO(D_RO(c)->free_idx_tail)->idx_free;
	TOID(struct block) block_with_free = block_get_by_idx(c, idx);

	if (!TOID_IS_NULL(D_RO(D_RO(c)->free_idx_tail)->prev)) {
		pmemobj_tx_add_range_direct(&D_RO(c)->free_idx_tail,
						sizeof(TOID(struct free_idx)));
		D_RW(c)->free_idx_tail = D_RO(D_RO(c)->free_idx_tail)->prev;
		pmemobj_tx_add_range_direct(&D_RO(D_RO(c)->free_idx_tail)->next,
						sizeof(TOID(struct free_idx)));
		D_RW(D_RW(c)->free_idx_tail)->next = TOID_NULL(struct free_idx);
	} else {
		D_RW(c)->free_idx_tail = TOID_NULL(struct free_idx);
	}
	D_RW(c)->free_idx_count--;
	pmemobj_tx_add_range_direct(&D_RO(block_with_free)->
				free_idx_count_block, sizeof(TOID(size_t)));
	D_RW(block_with_free)->free_idx_count_block--;

	return idx;
}

/*
 * blocks_nr_update -- updates block numbers after block removal
 */
static void
blocks_nr_update(TOID(struct colony) c, TOID(struct block) block_to_update)
{
	assert(pmemobj_tx_stage() == TX_STAGE_WORK);

	for (size_t i = D_RO(block_to_update)->block_nr; i < D_RO(c)->
							block_count - 1; i++) {
		pmemobj_tx_add_range_direct(&D_RO(block_to_update)->block_nr,
								sizeof(size_t));
		D_RW(block_to_update)->block_nr--;
		block_to_update = D_RO(block_to_update)->next;
	}
}

/*
 * free_blocks_delete -- destructor of all free blocks
 */
static void
free_blocks_delete(PMEMobjpool *pop, TOID(struct colony) *c)
{
	TOID(struct free_block) block_del = D_RO(*c)->free_block_tail;

	TX_BEGIN(pop) {
		assert(!TOID_IS_NULL(D_RO(*c)->free_block_tail));
		TX_ADD(*c);
		/* for each free block except the first one */
		while (!TOID_IS_NULL(D_RO(block_del)->prev)) {
			free_idxes_update_after_removal_free_block(*c,
				D_RW(D_RW(block_del)->block_free)->block_nr);
			/* for the first block in colony */
			if (TOID_EQUALS(D_RO(block_del)->block_free, D_RO(*c)->
								block_head)) {
				D_RW(*c)->block_head = D_RO(D_RO(*c)->
							block_head)->next;
				blocks_nr_update(*c, D_RO(D_RO(block_del)->
							block_free)->next);
			/* sets pointers if it's not the last block in colony */
			} else if (!TOID_EQUALS(D_RO(block_del)->
					block_free, D_RO(*c)->block_tail)) {
				pmemobj_tx_add_range_direct(
					&D_RO(D_RO(D_RO(block_del)->block_free)
								->prev)->next,
						sizeof(TOID(struct block)));
				D_RW(D_RW(D_RW(block_del)->block_free)->prev)->
					next = D_RO(D_RO(block_del)->block_free)
									->next;
				pmemobj_tx_add_range_direct(
					&D_RO(D_RO(D_RO(block_del)->block_free)
								->next)->prev,
						sizeof(TOID(struct block)));
				D_RW(D_RW(D_RW(block_del)->block_free)->next)->
					prev = D_RO(D_RO(block_del)->block_free)
									->prev;
				blocks_nr_update(*c, D_RO(D_RO(block_del)->
							block_free)->next);
			/* sets pointer if it is the last block in colony */
			} else {
				pmemobj_tx_add_range_direct(
					&D_RO(D_RO(D_RO(block_del)->block_free)
								->prev)->next,
						sizeof(TOID(struct block)));
				D_RW(D_RW(D_RW(block_del)->block_free)->prev)->
						next = TOID_NULL(struct block);
				pmemobj_tx_add_range_direct(&D_RO(*c)->
					block_tail, sizeof(TOID(struct block)));
				D_RW(*c)->block_tail = D_RO(D_RO(block_del)->
							block_free)->prev;
			}
			D_RW(*c)->block_count--;
			table_delete(pop, D_RO(D_RO(block_del)->block_free)->
									table);
			block_del = D_RO(block_del)->prev;
			TX_FREE(D_RO(D_RO(block_del)->next)->block_free);
		}
		/* for the first free block but not the first in colony */
		if (!TOID_EQUALS(D_RO(block_del)->block_free, D_RO(*c)->
			block_head)) {
			/* sets pointers if it's not the last block in colony */
			if (!TOID_EQUALS(D_RO(block_del)->block_free,
							D_RO(*c)->block_tail)) {
				pmemobj_tx_add_range_direct(&D_RO(D_RO(
					D_RO(block_del)->block_free)->prev)->
					next, sizeof(TOID(struct block)));
				D_RW(D_RW(D_RW(block_del)->block_free)->prev)->
					next = D_RO(D_RO(block_del)->block_free)
									->next;
				pmemobj_tx_add_range_direct(&D_RO(D_RO(
					D_RO(block_del)->block_free)->next)->
					prev, sizeof(TOID(struct block)));
				D_RW(D_RW(D_RW(block_del)->block_free)->next)->
					prev = D_RO(D_RO(block_del)->block_free)
									->prev;
			/* sets pointer if it is the last block in colony */
			} else {
				pmemobj_tx_add_range_direct(
					&D_RO(D_RO(D_RO(block_del)->block_free)
								->prev)->next,
						sizeof(TOID(struct block)));
				D_RW(D_RW(D_RW(block_del)->block_free)->prev)->
					next = TOID_NULL(struct block);
				D_RW(*c)->block_tail = D_RO(D_RO(block_del)->
							block_free)->prev;
			}
		}

		/* for the first block in colony */
		if (TOID_EQUALS(D_RO(block_del)->block_free, D_RO(*c)->
								block_head)) {
			D_RW(*c)->block_head = D_RO(D_RO(*c)->block_head)->next;
		}

		/* updates block numbers if it was not the last in colony */
		if (!TOID_IS_NULL(D_RO(D_RO(block_del)->block_free)->next))
			blocks_nr_update(*c, D_RO(D_RO(block_del)->block_free)->
									next);
		D_RW(*c)->block_count--;
		table_delete(pop, D_RO(D_RO(block_del)->block_free)->table);
		free_idxes_update_after_removal_free_block(*c,
				D_RO(D_RO(block_del)->block_free)->block_nr);
		TX_FREE(D_RO(block_del)->block_free);
		D_RW(*c)->colony_capacity -= D_RO(*c)->block_capacity * D_RO(*c)
							->free_block_count;
		D_RW(*c)->free_block_tail = TOID_NULL(struct free_block);
		D_RW(*c)->free_block_count = 0;
	} TX_ONABORT {
		fprintf(stderr, "%s: transaction aborted: %s\n", __func__,
							pmemobj_errormsg());
		abort();
	} TX_END
}


/*
 * free_blocks_list_delete -- destructor of the list of free blocks (blocks of
 * holes)
 */
static void
free_blocks_list_delete(PMEMobjpool *pop, TOID(struct colony) *c)
{
	TOID(struct free_block) block_del = D_RO(*c)->free_block_tail;

	TX_BEGIN(pop) {
		TX_ADD(*c);

		while (!TOID_IS_NULL(D_RO(block_del)->prev)) {
			block_del = D_RO(block_del)->prev;
			TX_FREE(D_RO(block_del)->next);
		}
		D_RW(*c)->free_block_count = 0;
		D_RW(*c)->free_block_tail = TOID_NULL(struct free_block);
	} TX_ONABORT {
		fprintf(stderr, "%s: transaction aborted: %s\n", __func__,
							pmemobj_errormsg());
		abort();
	} TX_END
}

/*
 * free_idxs_list_delete -- destructor of the list with free indexes (holes)
 */
static void
free_idxes_list_delete(PMEMobjpool *pop, TOID(struct colony) *c)
{
	TOID(struct free_idx) idx_del = D_RO(*c)->free_idx_tail;

	TX_BEGIN(pop) {
		TX_ADD(*c);

		while (!TOID_IS_NULL(D_RO(idx_del)->prev)) {
			idx_del = D_RO(idx_del)->prev;
			TX_FREE(D_RO(idx_del)->next);
		}
		D_RW(*c)->free_idx_count = 0;
		D_RW(*c)->free_idx_tail = TOID_NULL(struct free_idx);
	} TX_ONABORT {
		fprintf(stderr, "%s: transaction aborted: %s\n", __func__,
							pmemobj_errormsg());
		abort();
	} TX_END
}

/*
 * insert_item -- inserts the item into the colony;
 * order of inserting items:
 *	- if it exists - a free address (hole) from the list of free addresses
 *		(LIFO), free address means the one where was previously inserted
 *		and removed item
 *	- if it exists - a first address from the last free block (block of
 *		holes) from the list of free blocks (LIFO), the rest of the
 *		addresses from this block is transferred to the list of free
 *		addresses (holes)
 *	- if it exists - a first unoccupied address in colony, unoccupied
 *		address means one that has not been used (insertion, removal)
 *		before, it is always in the newest block
 *	- a new block is created (with a new table), the first unoccupied
 *		address is taken
 */
static size_t
insert_item(PMEMobjpool *pop, TOID(struct colony) c, void *item)
{
	size_t colony_idx = 0;

	TX_BEGIN(pop) {
		TX_ADD(c);

		/* if is a vacant in the colony */
		if (size_get(c) != capacity_get(c)) {
			/* if is a free address (hole) */
			if (D_RO(c)->free_idx_count) {
				colony_idx = free_idx_take_from(pop, c);
				insert_at_idx(pop, c, colony_idx, item);
			/* if is a free block (block of holes) */
			} else if (D_RO(c)->free_block_count) {
				colony_idx = free_block_take_from(pop, c);
				insert_at_idx(pop, c, colony_idx, item);
			/* if is an unoccupied address in the newest block */
			} else {
				colony_idx = unoccupied_idx_get(c);
				insert_at_idx(pop, c, colony_idx, item);
				pmemobj_tx_add_range_direct(&D_RO(D_RO(c)->
					block_tail)->idx_last, sizeof(size_t));
				D_RW(D_RW(c)->block_tail)->idx_last++;
			}
		/* if non of the above creates a new block */
		} else {
			block_constructor(pop, c);
			colony_idx = D_RO(c)->colony_size;
			insert_at_idx(pop, c, colony_idx, item);
			D_RW(D_RW(c)->block_tail)->idx_last++;
		}
		D_RW(c)->colony_size++;
	} TX_ONABORT {
		fprintf(stderr, "%s: transaction aborted: %s\n", __func__,
							pmemobj_errormsg());
		abort();
	} TX_END

	return colony_idx;
}

/*
 * remove_item -- removes the item element from the colony;
 * free address goes to the end of the list of free addresses (holes), if the
 * whole block (to which the index belongs) is empty (all elements were first
 * inserted,then removed), it goes to the end of the list of free blocks
 * (these individual indexes which belong to the block are removed from the
 * list of free addresses)
 */
static int
remove_item(PMEMobjpool *pop, TOID(struct colony) c, size_t colony_idx)
{
	if (colony_idx > D_RO(c)->colony_capacity - 1) {
		printf("\nthis index (%zu) is out of range\n", colony_idx);

		return -1;
	}
	TOID(struct block) block_with_idx = block_get_by_idx(c, colony_idx);
	TOID(size_t) jumps;
	TOID_ASSIGN(jumps, D_RO(D_RO(block_with_idx)->table)->jumps);

	size_t block_idx = colony_idx % D_RO(c)->block_capacity;
	if (D_RO(jumps)[block_idx] != 0) {
		printf("\nthe item from this index (%zu) does not exist\n",
								colony_idx);

		return -1;
	}
	TX_BEGIN(pop) {
		TX_ADD(c);

		free_idx_add_to(pop, c, colony_idx);
		pmemobj_tx_add_range_direct(&D_RO(c)->colony_size,
								sizeof(size_t));
		D_RW(c)->colony_size--;
	} TX_ONABORT {
		fprintf(stderr, "%s: transaction aborted: %s\n", __func__,
							pmemobj_errormsg());
		abort();
	} TX_END

		return 0;
}

/*
 * colony_create -- creates empty colony;
 * the first allocation of the block takes place when we want to insert the
 * first item, next if necessary
 */
static int
colony_create(PMEMobjpool *pop, TOID(struct colony) *c, const char *colony_name,
				enum array_types type, size_t block_capacity)
{
	if (sizeof(type) * block_capacity > PMEMOBJ_MAX_ALLOC_SIZE) {
		fprintf(stderr, "\nalloc failed: requested size greater than "
			"PMEMOBJ_MAX_ALLOC_SIZE\n");

		return -1;
	}
	TX_BEGIN(pop) {
		TX_ADD(*c);

		strncpy(D_RW(*c)->name, colony_name, MAX_BUFFLEN);
		D_RW(*c)->name[MAX_BUFFLEN - 1] = '\0';
		D_RW(*c)->element_type = type;
		D_RW(*c)->block_capacity = block_capacity;
		D_RW(*c)->block_count = 0;
		D_RW(*c)->colony_capacity = 0;
		D_RW(*c)->colony_size = 0;
		D_RW(*c)->free_idx_count = 0;
		D_RW(*c)->free_block_count = 0;

		D_RW(*c)->block_head = TOID_NULL(struct block);
		D_RW(*c)->block_tail = TOID_NULL(struct block);
		D_RW(*c)->free_idx_tail = TOID_NULL(struct free_idx);
		D_RW(*c)->free_block_tail = TOID_NULL(struct free_block);
	} TX_ONABORT {
		fprintf(stderr, "%s: transaction aborted: %s\n", __func__,
		pmemobj_errormsg());
	abort();
	} TX_END

		return 0;
}

/*
 * colony_delete -- deletes colony
 */
static int
colony_delete(PMEMobjpool *pop, TOID(struct colony) *c)
{
	TX_BEGIN(pop) {
		TX_ADD(*c);
		if (!TOID_IS_NULL(D_RO(*c)->block_tail))
			blocks_delete(pop, c);
		if (!TOID_IS_NULL(D_RO(*c)->free_idx_tail))
			free_idxes_list_delete(pop, c);
		if (!TOID_IS_NULL(D_RO(*c)->free_block_tail))
			free_blocks_list_delete(pop, c);
		D_RW(*c)->block_capacity = 0;
	} TX_ONABORT {
		fprintf(stderr, "%s: transaction aborted: %s\n", __func__,
							pmemobj_errormsg());
		abort();
	} TX_END

		return 0;
}
/*
 * find_colony -- returns the handle to the colony object with proper name
 */
static TOID(struct colony)
find_colony(PMEMobjpool *pop, const char *name)
{
	TOID(struct colony) col;

	POBJ_FOREACH_TYPE(pop, col) {
		if (strncmp(D_RO(col)->name, name, MAX_BUFFLEN) == 0)

			return col;
	}

	return TOID_NULL(struct colony);
}

/*
 * colony_print_content -- prints info about the content of the colony
 */
static void
colony_print_content(TOID(struct colony) c)
{
	printf("\n");
	printf("colony: block_capacity: %zu\n", D_RO(c)->block_capacity);
	printf("colony: block_count: %zu\n", D_RO(c)->block_count);
	printf("colony: colony_size: %zu\n", D_RO(c)->colony_size);
	printf("colony: colony_capacity: %zu\n", D_RO(c)->colony_capacity);
	printf("colony: free_idx_count: %zu\n", D_RO(c)->free_idx_count);
	printf("colony: free_block_count: %zu\n", D_RO(c)->free_block_count);
	printf("\n");
}



/*
 * colony_pmemoid_print -- prints the colony if the type is PMEMoid
 */
static void
colony_pmemoid_print(TOID(struct colony) c)
{
	TOID(struct block) b = D_RO(c)->block_head;
	TOID(size_t) jumps;
	TOID(PMEMoid) items;

	for (size_t i = 0; i < D_RO(c)->block_count; i++) {
		TOID_ASSIGN(jumps, D_RO(D_RO(b)->table)->jumps);
		TOID_ASSIGN(items, D_RO(D_RO(b)->table)->items);

		for (size_t j = D_RO(jumps)[0]; j < D_RO(c)->block_capacity;
									j++) {
			if (D_RO(jumps)[j] != 0)
				continue;
			size_t col_idx = i * D_RO(c)->block_capacity + j;
			printf("col_idx: %zu\t\tuuid: %" PRIu64 "\t\toff: "
			"%" PRIu64 "\n", col_idx, D_RO(items)[j].pool_uuid_lo,
							D_RO(items)[j].off);
			j += D_RO(jumps)[j];
		}
		b = D_RO(b)->next;
	}
}

/*
 * colony_print -- calls the colony printing function for the suitable type
 */
static void
colony_print(TOID(struct colony) c)
{
	if (D_RO(c)->element_type == INT_ARRAY_TYPE)
		colony_int_print(c);
	else if (D_RO(c)->element_type == PMEMOID_ARRAY_TYPE)
		colony_pmemoid_print(c);
}

/* available colony operations */
enum colony_op {
	UNKNOWN_COLONY_OP,

	COLONY_CREATE,
	COLONY_INSERT_INT,
	COLONY_INSERT_PMEMOID,
	COLONY_INSERT_INT_FROM_FILE,
	COLONY_INSERT_PMEMOID_FROM_FILE,
	COLONY_REMOVE_ITEM,
	COLONY_REMOVE_RANGE,
	COLONY_PRINT_CONTENT,
	COLONY_PRINT,
	COLONY_DELETE_FREE_BLOCKS,
	COLONY_DELETE,

	MAX_COLONY_OP
};

/* colony operations strings */
static const char *ops_str[MAX_COLONY_OP] = { "", "create-colony", "insert-int",
	"insert-pmemoid", "insert-int-from-file", "insert-pmemoid-from-file",
	"remove-item", "remove-range", "print-content", "print-colony", "delete"
					"-free-blocks", "delete-colony" };

/*
 * parse_colony_op -- parses the operation string and returns matching colony_op
 */
static enum colony_op
colony_op_parse(const char *str)
{
	for (int i = 0; i < MAX_COLONY_OP; ++i)
		if (strcmp(str, ops_str[i]) == 0)
			return (enum colony_op)i;

	return UNKNOWN_COLONY_OP;
}

int
main(int argc, char *argv[])
{
	TOID(struct colony) col;
	enum colony_op op;

	if (argc < 3 || (op = colony_op_parse(argv[2])) == UNKNOWN_COLONY_OP) {
		printf("\nusage:\n\t<filename> create-colony <colony-name> "
			"<element-type> <block-capacity>\n\t<filename> insert-"
			"int <colony-name> <item>\n\t<filename> insert-pmemoid "
			"<colony-name> <item-uuid> <item-off>\n\t<filename> "
			"insert-int-from-file <colony-name> <path>\n\t"
			"<filename> insert-pmemoid-from-file <colony-name> "
			"<path>\n\t<filename> remove-item <colony-name> <index>"
			"\n\t<filename> remove-range <colony-name> <index-from>"
			" <index-to>\n\t<filename> print-content <colony-name>"
			"\n\t<filename> print-colony <colony-name>\n\t"
			"<filename> delete-free-blocks <colony-name>\n\t"
			"<filename> delete-colony <colony-name>\n");

		return 0;
	}

	PMEMobjpool *pop = NULL;
	const char *path = argv[1];

	if (file_exists(path) != 0) {
		if ((pop = pmemobj_create(path, POBJ_LAYOUT_NAME(colony),
				PMEMOBJ_MIN_POOL, CREATE_MODE_RW)) == NULL) {
			printf("\nfailed to create pool\n");

			return 1;
		}
	} else {
		if ((pop = pmemobj_open(path, POBJ_LAYOUT_NAME(colony)))
								== NULL) {
			printf("\nfailed to open pool\n");

			return 1;
		}
	}

	const char *name = argv[3];

	switch (op) {
	case COLONY_CREATE:
	{
		if (argc != 6) {
			printf("\nusage:\n\t./colony <filename> create-colony "
				"<colony-name> <element-type> <block-capacity>"
									"\n");
			break;
		}
		col = find_colony(pop, argv[3]);
		if (!TOID_IS_NULL(col))
			POBJ_FREE(&col);
		POBJ_ZNEW(pop, &col, struct colony);
		enum array_types element_type = get_type(argv[4]);
		size_t capacity = (size_t)atoll(argv[5]);
		if (colony_create(pop, &col, name, element_type, capacity) != 0)
			printf("\nfailed to create a colony\n");
	}
	break;

	case COLONY_INSERT_INT:
	{
		if (argc != 5) {
			printf("\nusage:\n\t./colony <filename> insert-int "
						"<colony-name> <item>\n");
			break;
		}
		col = find_colony(pop, name);
		if (TOID_IS_NULL(col)) {
			printf("\n%s doesn't exist\n", name);
			break;
		}
		if (D_RO(col)->element_type == PMEMOID_ARRAY_TYPE)
			break;

		int it = atoi(argv[4]);
		insert_item(pop, col, &it);
	}
	break;

	case COLONY_INSERT_PMEMOID:
	{
		if (argc != 6) {
			printf("\nusage:\n\t./colony <filename> insert-pmemoid "
				"<colony-name> <item-uuid> <item-off>\n");
			break;
		}
		col = find_colony(pop, name);
		if (TOID_IS_NULL(col)) {
			printf("\n%s doesn't exist\n", name);
			break;
		}
		if (D_RO(col)->element_type == INT_ARRAY_TYPE)
			break;

		PMEMoid p = OID_NULL;
		p.pool_uuid_lo = atoll(argv[4]);
		p.off = atoll(argv[5]);
		insert_item(pop, col, &p);
	}
	break;

	case COLONY_INSERT_INT_FROM_FILE:
	{
		if (argc != 5) {
			printf("\nusage:\n\t./colony <filename> insert-int-from"
						"-file <colony-name> <path>\n");
			break;
		}
		col = find_colony(pop, name);
		if (TOID_IS_NULL(col)) {
			printf("\n%s doesn't exist\n", name);
			break;
		}
		const char *file_name = argv[4];
		FILE *pFile = fopen(file_name, "r");
		if (!pFile) {
			printf("\nfailed to open file\n");
			break;
		}
		int it;
		while (fscanf(pFile, "%d", &it) == 1)
			insert_item(pop, col, &it);
		fclose(pFile);
	}
	break;

	case COLONY_INSERT_PMEMOID_FROM_FILE:
	{
		if (argc != 5) {
			printf("\nusage:\n\t./colony <filename> insert-pmemoid-"
					"from-file <colony-name> <path>\n");
			break;
		}
		col = find_colony(pop, name);
		if (TOID_IS_NULL(col)) {
			printf("\n%s doesn't exist\n", name);
			break;
		}
		const char *file_name = argv[4];
		FILE *pFile = fopen(file_name, "r");
		if (!pFile) {
			printf("\nfailed to open file\n");
			break;
		}
		PMEMoid p = OID_NULL;
		while (fscanf(pFile, "%" SCNu64	" %" SCNu64, &p.pool_uuid_lo,
								&p.off) == 2)
			insert_item(pop, col, &p);
		fclose(pFile);
	}
	break;

	case COLONY_REMOVE_ITEM:
	{
		if (argc != 5) {
			printf("\nusage:\n\t./colony <filename> remove-item "
						"<colony-name> <index>\n");
			break;
		}
		col = find_colony(pop, name);
		if (TOID_IS_NULL(col)) {
			printf("\n%s doesn't exist\n", name);
			break;
		}
		size_t idx = atoll(argv[4]);
		remove_item(pop, col, idx);
	}
	break;

	case COLONY_REMOVE_RANGE:
	{
		if (argc != 6) {
			printf("\nusage:\n\t./colony <filename> remove-range "
				"<colony-name> <index-from> <index-to>\n");
			break;
		}
		col = find_colony(pop, name);
		if (TOID_IS_NULL(col)) {
			printf("\n%s doesn't exist\n", name);
			break;
		}
		size_t idx_from = atoll(argv[4]);
		size_t idx_to = atoll(argv[5]);
		for (size_t i = idx_from; i <= idx_to; i++)
			remove_item(pop, col, i);
	}
	break;

	case COLONY_PRINT_CONTENT:
	{
		if (argc != 4) {
			printf("\nusage:\n\t./colony <filename> print-content "
							"<colony-name>\n");
			break;
		}
		col = find_colony(pop, argv[3]);
		if (TOID_IS_NULL(col)) {
			printf("\n%s doesn't exist\n", name);
			break;
		}
		colony_print_content(col);
	}
	break;

	case COLONY_PRINT:
	{
		if (argc != 4) {
			printf("\nusage:\n\t./colony <filename> print-colony "
							"<colony-name>\n");
			break;
		}
		col = find_colony(pop, name);
		if (TOID_IS_NULL(col)) {
			printf("\n%s doesn't exist\n", argv[3]);
			break;
		}
		colony_print(col);
	}
	break;

	case COLONY_DELETE_FREE_BLOCKS:
	{
		if (argc != 4) {
			printf("\nusage:\n\t./colony <filename> delete-free-"
						"blocks <colony-name>\n");
			break;
		}
		col = find_colony(pop, argv[3]);
		if (TOID_IS_NULL(col)) {
			printf("\n%s doesn't exist\n", name);
			break;
		}
		if (!TOID_IS_NULL(D_RO(col)->free_block_tail))
			free_blocks_delete(pop, &col);
	}
	break;

	case COLONY_DELETE:
	{
		if (argc != 4) {
			printf("\nusage:\n\t./colony <filename> delete-colony "
							"<colony-name>\n");
			break;
		}
		col = find_colony(pop, argv[3]);
		if (TOID_IS_NULL(col)) {
			printf("\n%s doesn't exist\n", name);
			break;
		}
		if (colony_delete(pop, &col) != 0)
			printf("\nfailed to delete colony\n");
		else
			POBJ_FREE(&col);
	}
	break;

	default:
		assert(0);
	}

	pmemobj_close(pop);

	return 0;
}

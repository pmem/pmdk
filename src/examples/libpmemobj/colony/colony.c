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

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <libpmemobj.h>

POBJ_LAYOUT_BEGIN(colony);
POBJ_LAYOUT_TOID(colony, struct colony);
POBJ_LAYOUT_END(colony);

POBJ_LAYOUT_BEGIN(block);
POBJ_LAYOUT_TOID(block, struct block);
POBJ_LAYOUT_TOID(block, TOID(struct block));
POBJ_LAYOUT_END(block);

POBJ_LAYOUT_BEGIN(free_block);
POBJ_LAYOUT_TOID(free_block, struct free_block);
POBJ_LAYOUT_END(free_block);

POBJ_LAYOUT_BEGIN(free_idx);
POBJ_LAYOUT_TOID(free_idx, struct free_idx);
POBJ_LAYOUT_END(free_idx);

POBJ_LAYOUT_BEGIN(table);
POBJ_LAYOUT_TOID(table, int);
POBJ_LAYOUT_END(table);

enum array_types {
	UNKNOWN_ARRAY_TYPE,
	INT_ARRAY_TYPE,
	PMEMOID_ARRAY_TYPE,
	TOID_ARRAY_TYPE,
	MAX_ARRAY_TYPE
};

struct block {
	/* indicates the beginning of table */
	PMEMoid table;

	/* number of occupied elements */
	size_t block_size;
	/* number of block in colony */
	size_t block_nr;

	/* index of last added element */
	size_t idx_last;
	/* number of free (removed) elements */
	size_t free_elem;

	/* pointer on the previous block */
	TOID(struct block) prev;
	/* pointer on the next block */
	TOID(struct block) next;
};

struct free_idx {
	/* index (in colony) of free element */
	size_t idx;

	/* pointer on the previous free element */
	// free_idx *prev;
	/* pointer on the next free element */
	// free_idx *next;
};

struct free_block {
	/* pointer on the free block */
	TOID(struct block) block_free;
	/* pointer on the next free block */
	// TOID(struct free_block) next;
};

struct colony {
	/* type of elements in colony */
	enum array_types element_type;

	/* pointer on the first block */
	TOID(struct block) block_head;
	/* pointer on the last block */
	TOID(struct block) block_tail;
	/* number of elements in each block */
	size_t block_capacity;
	/* number of blocks in colony */
	size_t block_count;
	/* number of occupied elements */
	size_t colony_size;
	/* total capacity = block_capacity * block_count */
	size_t colony_capacity;

	/* number of free elements (holes) */
	size_t free_idx_count;
	/* pointer on the first free element */
	TOID(struct free_idx) free_idx_head;

	/* number of free blocks */
	size_t free_block_count;
	/* pointer on the first free block */
	TOID(struct free_block) free_block_head;
};

/*
 * get type -- parses argument given as type of colony
 */
static enum array_types
get_type(const char *type_name)
{
	const char *names[MAX_ARRAY_TYPE] = {"", "int", "PMEMoid", "TOID"};
	enum array_types type;

	for (type = (enum array_types)(MAX_ARRAY_TYPE - 1);
			type > UNKNOWN_ARRAY_TYPE;
			type = (enum array_types)(type - 1)) {
		if (strcmp(names[type], type_name) == 0)
			break;
	}
	if (type == UNKNOWN_ARRAY_TYPE)
		fprintf(stderr, "unknown type: %s\n", type_name);
	return type;
}

/*
 * file_exists -- checks if the file exists
 */
static int
file_exists(const char *filename)
{
	struct stat buffer;
	int exist = stat(filename, &buffer);

	return exist;
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
 * block_idx_get -- returns the index in a block from the index in colony
 */
static size_t
block_idx_get_by_idx(TOID(struct colony) c, size_t colony_idx)
{
	size_t block_nr = colony_idx / D_RO(c)->block_capacity;
	size_t block_idx = colony_idx - (block_nr) * D_RO(c)->block_capacity;

	return block_idx;
}

/*
 * block_get_by_idx -- returns the pointer to the block in which the index is
 */
static TOID(struct block)
block_get_by_idx(TOID(struct colony) c, size_t colony_idx)
{
	size_t block_nr = colony_idx / D_RO(c)->block_capacity;

	/* sets the pointer to the block */
	TOID(struct block) block_with_idx = D_RO(c)->block_head;
	for (size_t i = 0; i < block_nr; i++) {
		block_with_idx = D_RO(block_with_idx)->next;
	}

	return block_with_idx;
}

/*
 * block_get_by_nr -- returns the pointer to the block with number
 */
static TOID(struct block)
block_get_by_nr(TOID(struct colony) c, size_t block_nr)
{
	TOID(struct block) block_with_nr = D_RO(c)->block_head;
	for (size_t i = 1; i < block_nr; i++) {
		block_with_nr = D_RO(block_with_nr)->next;
	}

	return block_with_nr;
}

/*
 * int_insert_at -- inserts the integer element in the colony at colony_idx
 */
static void
int_insert_at_idx(PMEMobjpool *pop, TOID(struct colony) c, size_t colony_idx,
								int element)
{
	/* sets the pointer to the table in which is a free element */
	TOID(struct block) block_with_free = block_get_by_idx(c, colony_idx);
	TOID(int) table;
	TOID_ASSIGN(table, D_RW(block_with_free)->table);

	size_t block_idx = block_idx_get_by_idx(c, colony_idx);

	D_RW(table)[block_idx] = element;
	D_RW(block_with_free)->block_size++;
}

/*
 * table_int_create -- allocates the table of integers in the block
 */
static int
table_int_create(PMEMobjpool *pop, TOID(struct colony) c, TOID(struct block) b)
{
	TOID(int) table;
	TOID_ASSIGN(table, D_RO(b)->table);

	return POBJ_ALLOC(pop, &table, int, sizeof(D_RO(c)->element_type) *
					(D_RO(c)->block_capacity), NULL, NULL);
}

/*
 * table_int_delete -- frees the table of integers
 */
static void
table_int_delete(PMEMoid table_del)
{
	TOID(int) table;
	TOID_ASSIGN(table, table_del);
	/*
	 * When there is persistent table of simple type allocated,
	 * there is enough to de-allocate persistent pointer
	 */
	POBJ_FREE(&table);
}

/*
 * block_constructor -- constructor of the one block
 */
static int
block_constructor(PMEMobjpool *pop, TOID(struct colony) c)
{
	POBJ_NEW(pop, &(D_RO(D_RO(c)->block_tail)->next), struct block, NULL,
									NULL);
	POBJ_NEW(pop, &(D_RO(D_RO(c)->free_block_head)[D_RO(c)->
				free_block_count]), struct free_block, NULL,
									NULL);
	POBJ_NEW(pop, &(D_RO(D_RO(c)->free_idx_head)[D_RO(c)->free_idx_count]),
						struct free_idx, NULL, NULL);

	TOID(struct block) b = D_RO(D_RO(c)->block_tail)->next;

	D_RW(b)->block_nr = D_RO(c)->block_count + 1;

	table_int_create(pop, c, b);

	D_RW(b)->block_size = 0;
	D_RW(b)->idx_last = -1;
	D_RW(b)->free_elem = 0;

	/* if it is a first block */
	if (TOID_IS_NULL(D_RO(c)->block_head)) {
		D_RW(b)->prev = TOID_NULL(struct block);
		D_RW(b)->block_nr = 1;
		D_RW(b)->next = TOID_NULL(struct block);
		D_RW(c)->block_head = b;
		D_RW(c)->block_tail = b;
	} else {
		D_RW(b)->prev = D_RO(c)->block_tail;
		D_RW(b)->block_nr = D_RO(D_RO(b)->prev)->block_nr + 1;
		D_RW(b)->next = TOID_NULL(struct block);
		D_RW(D_RW(c)->block_tail)->next = b;
		D_RW(c)->block_tail = b;
	}

	D_RW(c)->colony_capacity += D_RO(c)->block_capacity;
	D_RW(c)->block_count++;

	return 0;
}

/*
 * blocks_delete -- destructor of the blocks
 */
static int
blocks_delete(PMEMobjpool *pop, TOID(struct colony) c)
{
	for (size_t i = 0; i < D_RO(c)->block_count; i--) {
		/* sets the pointer on the block */
		TOID(struct block) block_del = block_get_by_nr(c, i + 1);

		table_int_delete(D_RW(block_del)->table);

		POBJ_FREE(&block_del);
	}

	D_RW(c)->block_head = TOID_NULL(struct block);
	D_RW(c)->block_tail = TOID_NULL(struct block);
	D_RW(c)->free_block_head = TOID_NULL(struct free_block);

	D_RW(c)->block_count = 0;

	return 0;
}

/*
 * free_idx_get -- returns first unoccupied (!= removed) index in colony
 */
static size_t
free_idx_get(TOID(struct colony) c)
{
	size_t block_idx = D_RO(D_RO(c)->block_tail)->idx_last + 1;
	size_t colony_idx = (D_RO(c)->block_count - 1) * D_RO(c)->block_capacity
								+ block_idx;

	return colony_idx;
}

/*
 * free_blocks_delete -- destructor of the free blocks
 */
static int
free_blocks_delete(PMEMobjpool *pop, TOID(struct colony) c)
{
	size_t block_del_nr;

	TX_BEGIN(pop) {
		TX_ADD_FIELD(c, block_count);
		TX_ADD_FIELD(c, colony_capacity);
		TX_ADD_FIELD(c, free_block_head);
		TX_ADD_FIELD(c, free_block_count);

		for (size_t i = D_RO(c)->free_block_count; i > 0; i--) {

			D_RW(c)->block_count--;
			D_RW(c)->colony_capacity -= D_RO(c)->block_capacity;

			/* sets "next" */
			D_RW(D_RW(D_RW(D_RW(c)->free_block_head)[i].block_free)
				->prev)->next = D_RO(D_RO(D_RO(c)->
				free_block_head)[i].block_free)->next;

			block_del_nr = D_RO(D_RO(D_RO(c)->free_block_head)[i].
							block_free)->block_nr;

			/* sets the pointer on the block */
			TOID(struct block) block_del = block_get_by_nr(c,
								block_del_nr);

			/* updates block numbers */
			for (size_t k = block_del_nr; k < D_RO(c)->block_count;
									k++) {
				D_RW(block_del)->block_nr--;
				block_del = D_RW(block_del)->next;
			}

			table_int_delete(D_RW(D_RW(D_RW(c)->free_block_head)[i].
							block_free)->table);

			TX_FREE(D_RO(D_RO(c)->free_block_head)[i].block_free);
		}

		D_RW(c)->free_block_head = TOID_NULL(struct free_block);

		D_RW(c)->free_block_count = 0;
	} TX_ONABORT {
		fprintf(stderr, "%s: transaction aborted: %s\n", __func__,
			pmemobj_errormsg());
		abort();
	} TX_END

	return 0;
}

/*
 * free_block_add_to -- adds the free block (all elements removed) to the list
 * with free blocks
 */
static int
free_block_add_to(PMEMobjpool *pop, TOID(struct colony) c, size_t block_nr)
{
	/* sets the pointer on the block */
	TOID(struct block) block_with_free = block_get_by_nr(c, block_nr);

	D_RW(D_RW(c)->free_block_head)[D_RO(c)->free_block_count].
						block_free = block_with_free;
	D_RW(c)->free_idx_count -= D_RO(c)->block_capacity;
	D_RW(c)->free_block_count++;

	return 0;
}

/*
 * free_idx_add_to -- saves the address where the removal was made
 */
static int
free_idx_add_to(PMEMobjpool *pop, TOID(struct colony) c, size_t idx)
{
	size_t block_nr = idx / D_RO(c)->block_capacity;

	/* inserts the address into the list of free addresses */
	D_RW(D_RW(c)->free_idx_head)[D_RO(c)->free_idx_count].idx = idx;
	D_RW(c)->free_idx_count++;

	/* sets the pointer on the block */
	TOID(struct block) block_with_free = block_get_by_idx(c, idx);

	D_RW(block_with_free)->free_elem++;

	/* if all elements are removed */
	if (D_RO(block_with_free)->free_elem == D_RO(c)->block_capacity)
		free_block_add_to(pop, c, block_nr);

	return 0;
}

/*
 * free_block_take_from -- takes the address from the list of free blocks to
 * insertion
 */
static size_t
free_block_take_from(PMEMobjpool *pop, TOID(struct colony) c)
{
	/* takes the first address from the last block from the list */
	size_t idx = (D_RO(D_RO(D_RO(c)->free_block_head)[D_RO(c)->
		free_block_count - 1].block_free)->block_nr - 1) * D_RO(c)->
								block_capacity;

	D_RW(D_RW(D_RW(c)->free_block_head)[D_RO(c)->free_block_count - 1].
						block_free)->free_elem = 0;
	D_RW(c)->free_block_count--;

	/* adds the rest of elements to the list of free addresses */
	for (size_t i = 1; i < D_RO(c)->block_capacity; i++) {
		free_idx_add_to(pop, c, idx + i);
	}

	/* if the list of free blocks is empty */
	if (D_RW(c)->free_block_count == 0)
		D_RW(c)->free_block_head = TOID_NULL(struct free_block);

	return idx;
}

/*
 * free_idx_take_from -- takes the address from the list of free addresses to
 * insertion
 */
static size_t
free_idx_take_from(PMEMobjpool *pop, TOID(struct colony) c)
{
	/* takes the last address from the list of free addresses */
	size_t idx = D_RO(D_RO(c)->free_idx_head)[D_RO(c)->free_idx_count - 1].
									idx;
	/* sets the pointer on the block */
	TOID(struct block) block_with_free = block_get_by_idx(c, idx);
	D_RW(block_with_free)->free_elem--;
	D_RW(c)->free_idx_count--;

	/* if the list of free addresses is empty */
	if (D_RW(c)->free_idx_count == 0)
		D_RW(c)->free_idx_head = TOID_NULL(struct free_idx);

	return idx;
}

/*
 * colony_create -- creates empty colony
 */
static int
colony_create(PMEMobjpool *pop, TOID(struct colony) c, enum array_types type,
							int block_capacity)
{
	if (sizeof(type) * block_capacity > PMEMOBJ_MAX_ALLOC_SIZE) {
		fprintf(stderr, "alloc failed: %s\n", pmemobj_errormsg());
		return -1;
	}

	TX_BEGIN(pop) {
		TX_ADD(c);

		D_RW(c)->element_type = type;
		D_RW(c)->block_capacity = block_capacity;
		D_RW(c)->block_count = 0;
		D_RW(c)->colony_capacity = 0;
		D_RW(c)->free_idx_count = 0;
		D_RW(c)->free_block_count = 0;

		D_RW(c)->block_head = TOID_NULL(struct block);
		D_RW(c)->block_tail = TOID_NULL(struct block);
		D_RW(c)->free_idx_head = TOID_NULL(struct free_idx);
		D_RW(c)->free_block_head = TOID_NULL(struct free_block);
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
colony_delete(PMEMobjpool *pop, TOID(struct colony) c)
{
	TX_BEGIN(pop) {
		TX_ADD(c);

		blocks_delete(pop, c);
	} TX_ONABORT {
		fprintf(stderr, "%s: transaction aborted: %s\n", __func__,
			pmemobj_errormsg());
		abort();
	} TX_END

	return 0;
}

/*
 * insert_int -- inserts the integer element into the colony
 */
static int
insert_int(PMEMobjpool *pop, TOID(struct colony) c, int element)
{
	int colony_idx;

	TX_BEGIN(pop) {
		TX_ADD(c);

		/* if is a vacant in the colony */
		if (size_get(c) == capacity_get(c)) {

			/* if is a free address (hole) */
			if (D_RO(c)->free_idx_count) {
				colony_idx = free_idx_take_from(pop, c);
				int_insert_at_idx(pop, c, colony_idx, element);

			/* if is a free block (block of holes) */
			} else if (D_RO(c)->free_block_count) {
				colony_idx = free_block_take_from(pop, c);
				int_insert_at_idx(pop, c, colony_idx, element);

			/* if is an unoccupied address in the newest block */
			} else {
				colony_idx = free_idx_get(c);
				int_insert_at_idx(pop, c, colony_idx, element);
				D_RW(D_RW(c)->block_tail)->idx_last++;
		}

		/* creates a new block */
		} else {
			block_constructor(pop, c);

			TOID(int) table;
			TOID_ASSIGN(table, D_RO(D_RO(c)->block_tail)->table);

			D_RW(table)[0] = element;
			D_RW(D_RW(c)->block_tail)->idx_last++;
			D_RW(D_RW(c)->block_tail)->block_size++;

			colony_idx = D_RO(c)->colony_size;
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
 * remove_int -- removes the integer element from the colony
 */
static int
remove_int(PMEMobjpool *pop, TOID(struct colony) c, size_t colony_idx)
{
	if (colony_idx >= D_RO(c)->colony_capacity) {
		printf("element does not exist\n");
		return -1;
	}

	/* sets the pointer on the block */
	TOID(struct block) block_with_elem = block_get_by_idx(c, colony_idx);

	TX_BEGIN(pop) {
		TX_ADD_FIELD(c, colony_size);

		D_RW(c)->colony_size--;
		D_RW(block_with_elem)->block_size--;

		free_idx_add_to(pop, c, colony_idx);
	} TX_ONABORT {
		fprintf(stderr, "%s: transaction aborted: %s\n", __func__,
							pmemobj_errormsg());
		abort();
	} TX_END

	return 0;
}

int
main(int argc, char *argv[])
{
	PMEMobjpool *pop;
	const char *path = argv[1];

	if (file_exists(path) != 0) {
		if ((pop = pmemobj_create(path, POBJ_LAYOUT_NAME(colony),
					PMEMOBJ_MIN_POOL, 0666)) == NULL) {
			printf("failed to create pool\n");
			return 1;
		}
	} else {
		if ((pop = pmemobj_open(path, POBJ_LAYOUT_NAME(colony)))
								== NULL) {
			printf("failed to open pool\n");
			return 1;
		}
	}

	enum array_types element_type = get_type(argv[2]);
	int capacity = atoll(argv[3]);

	TOID(struct colony) col;

	if (colony_create(pop, col, element_type, capacity) == 0)
		printf("create col\n");
	else printf("not create col\n");

	insert_int(pop, col, 4);

	remove_int(pop, col, 1);

	free_blocks_delete(pop, col);


	if (colony_delete(pop, col) == 0)
		printf("del col\n");
	else printf("not del col\n");

	pmemobj_close(pop);

	return 0;
}

// realloc changes capacity

// insert_range remove_range

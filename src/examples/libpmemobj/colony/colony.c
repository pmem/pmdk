#include <stdio.h>
#include <stdlib.h>
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

static PMEMobjpool *pop;

struct block {
	enum array_types type;
	PMEMoid table;	// indicates the beginning of table
	size_t capacity;	// number of elements (in each block the same)
	size_t occ_elem;	// number of occupied elements
	size_t last_elem;	// index of last added element
	size_t index_of_first_elem;	// index in colony
	struct block *previous;
	struct block *next;
	size_t number_in_colony;	// number of block in colony
};

struct free_address {
	size_t index;	// free index
	// size_t *next_occ;	// next occupied
};

struct free_block {
	struct block *this;
};

struct colony {
	enum array_types type;
	struct block *first_block;	// indicates the first el in first block
	struct block *last_block;	// the last el in last block
	size_t block_capacity;	// number of elem in each block
	size_t block_num;	// number of blocks
	size_t occ_elem;	// number of occupied elements
	size_t capacity;	// total capacity = block_capacity * blocks_num
	size_t free_elem;	// number of free addresses (holes)
	size_t free_blocks;	// number of free blocks
	struct free_address *free_addresses_head;	// first free address
	struct free_block *free_blocks_head;
};

/*
 * get type -- parse argument given as type of colony
 */
static enum array_types
get_type(const char *type_name)
{
	const char *names[MAX_ARRAY_TYPE] = {"", "int", "PMEMoid", "TOID"};
	enum array_types type;
	for (type = (enum array_types)(MAX_ARRAY_TYPE - 1);
		type > UNKNOWN_ARRAY_TYPE; type = (enum array_types)
								(type - 1)) {
		if (strcmp(names[type], type_name) == 0)
			break;
	}
	if (type == UNKNOWN_ARRAY_TYPE)
		fprintf(stderr, "unknown type: %s\n", type_name);
	return type;
}

/*
 * file_exists -- check if the file exist
 */
static int
file_exists(const char *filename)
{
	struct stat buffer;
	int exist = stat(filename, &buffer);

	return exist;
}

/*
 * create_table_int -- allocate table in new block
 */
static int
create_table_int(TOID(struct block) b)
{
	TOID(int) table;
	TOID_ASSIGN(table, D_RO(b)->table);

	D_RW(b)->index_of_first_elem = (D_RO(b)->number_in_colony - 1)
							* D_RO(b)->capacity;

	return POBJ_ALLOC(pop, &table, int,
			sizeof(D_RO(b)->type)*(D_RO(b)->capacity), NULL, NULL);
}

/*
 * delete_table_int -- free table from empty block
 */
static void
delete_table_int(PMEMoid table_del)
{
	TOID(int) table;
	TOID_ASSIGN(table, table_del);
	/*
	 * When there is persistent table of simple type allocated,
	 * there is enough to de-allocate persistent pointer
	 */
	POBJ_FREE(&table);

	// + przesuwanie indeksow
}

/*
 * block_constructor -- constructor of the one block
 */
static int
block_constructor(PMEMobjpool *pop, TOID(struct colony) c)
{
	TOID(struct block) b;

	D_RW(b)->number_in_colony = D_RO(c)->block_num + 1;

	create_table_int(b);

	D_RW(b)->type = D_RO(c)->type;
	D_RW(b)->capacity = D_RO(c)->block_capacity;
	D_RW(b)->occ_elem = 0;

	// set prev/next
	if (D_RO(c)->block_num == 0) {
		D_RW(b)->previous = NULL;
		D_RW(b)->next = NULL;
		D_RW(c)->first_block = D_RW(b);
		D_RW(c)->last_block = D_RW(b);
	} else {
		D_RW(b)->previous = D_RO(c)->last_block;
		D_RW(b)->next = NULL;
		D_RW(c)->last_block->next = D_RW(b);
		D_RW(c)->last_block = D_RW(b);
	}

	// update colony
	D_RW(c)->capacity += D_RO(b)->capacity;
	D_RW(c)->block_num++;

	return 0;
}

/*
 * delete_free_blocks -- destructor of the free blocks
 */
static int
delete_free_blocks(TOID(struct colony) c)
{
	for (size_t i = 0; i < D_RO(c)->free_blocks; i++) {

		D_RW(c)->block_num--;
		D_RW(c)->capacity -= D_RO(c)->block_capacity;

		D_RW(c)->free_blocks_head[i].this->previous =
					D_RW(c)->free_blocks_head[i].this->next;
		D_RW(c)->free_blocks_head[i].this->next =
				D_RW(c)->free_blocks_head[i].this->previous;


		delete_table_int(D_RW(c)->free_blocks_head[i].this->table);

		D_RW(c)->capacity -= D_RW(c)->block_capacity;
	}

	D_RW(c)->free_blocks_head = NULL;

	D_RW(c)->free_blocks = 0;

	return 0;
}

/*
 * add_to_free_block -- add block to stack with free blocks
 */
static int
add_to_free_block(TOID(struct colony) c, TOID(struct block) b)
{
	D_RW(c)->free_blocks_head[D_RW(c)->free_blocks].this = D_RW(b);

	D_RW(c)->free_elem -= D_RW(c)->block_capacity;

	D_RW(c)->free_blocks++;

	return 0;
}

/*
 * add_to_free -- save the address where remove was made
 */
static int
add_to_free_address(TOID(struct colony) c, size_t index)
{
	// insert address into struct free_address
	D_RW(c)->free_addresses_head[D_RW(c)->free_elem].index = index;

	D_RW(c)->free_elem++;

	// spr czy nie caly blok -> zapis do free_block

	return 0;
}

/*
 * take_from_free_address -- take the address to insert
 */
static size_t
take_from_free_address(TOID(struct colony) c)
{

	size_t index = D_RW(c)->free_addresses_head[D_RO(c)->
							free_elem - 1].index;
	D_RW(c)->free_elem--;

	// if is empty
	if (D_RW(c)->free_elem == 0)
		D_RW(c)->free_addresses_head = NULL;

	return index;
}

/*
 * take_from_free_block -- take the address to insert
 */
static size_t
take_from_free_block(TOID(struct colony) c)
{

	size_t index = D_RW(c)->free_blocks_head[D_RO(c)->
				free_blocks - 1].this->index_of_first_elem;
	D_RW(c)->free_blocks--;

	// add rest of elemrnts to free_addresses head
	for (size_t i = 1; i < D_RO(c)->block_capacity; i++) {
		add_to_free_address(c, index + i);
	}

	// if stack is empty
	if (D_RW(c)->free_blocks == 0)
		D_RW(c)->free_blocks_head = NULL;

	return index;
}

/*
 * create_colony -- create empty colony
 */
static int
create_colony(TOID(struct colony) c, enum array_types type, int block_capacity)
{
	if (type * block_capacity > PMEMOBJ_MAX_ALLOC_SIZE) {
				// czy MAX_SIZE dotyczy jednorazowej alokacji?
		printf("block_capacity > PMEMOBJ_MAX_ALLOC_SIZE\n");
		return -1;
	}

	D_RW(c)->type = type;
	D_RW(c)->block_capacity = block_capacity;
	D_RW(c)->block_num = 0;
	D_RW(c)->capacity = 0;
	D_RW(c)->free_elem = 0;
	D_RW(c)->free_blocks = 0;
	// D_RW(c)->index = 0;

	D_RW(c)->first_block = NULL;
	D_RW(c)->last_block = NULL;
	D_RW(c)->free_addresses_head = NULL;
	D_RW(c)->free_blocks_head = NULL;

	return 0;
}

/*
 * delete_colony -- delete colony
 */
static int
delete_colony(TOID(struct colony) c)
{
	// free

	return 0;
}

/*
 * insert_int -- insert an int element into colony
 */
static int
insert_int(TOID(struct colony) c, int element)
{
	TOID(int) table;
	int index_in_colony;

	// is vacant in colony
	if ((D_RO(c)->capacity - D_RO(c)->occ_elem) != 0) {

		// is unoccupied address (hole)
		if (D_RO(c)->free_elem != 0) {
			index_in_colony = take_from_free_address(c);
			size_t number_of_block = index_in_colony /
				D_RW(c)->block_capacity; // counted from 0
			struct block *block_where_is_free = D_RW(c)
								->first_block;

			for (size_t i = 0; i < number_of_block; i++) {
				block_where_is_free = block_where_is_free->next;
			}

			TOID_ASSIGN(table, block_where_is_free->table);
			int index_in_block = index_in_colony -
					(number_of_block) * D_RO(c)
							->block_capacity;
			D_RW(table)[index_in_block] = element;

			block_where_is_free->occ_elem++;

		// is unoccupied block (block of holes)
		} else if (D_RO(c)->free_blocks != 0) {
			index_in_colony = take_from_free_block(c);
			size_t number_of_block = index_in_colony /
				D_RW(c)->block_capacity; // counted from 0
			struct block *block_where_is_free = D_RW(c)
								->first_block;

			for (size_t i = 0; i < number_of_block; i++) {
				block_where_is_free = block_where_is_free->next;
			}

			TOID_ASSIGN(table, block_where_is_free->table);
			D_RW(table)[0] = element;

			block_where_is_free->occ_elem++;

		// is unoccupied address in the newest block
		} else {
			TOID_ASSIGN(table, D_RO(c)->last_block->table);

			D_RW(table)[D_RO(c)->last_block->last_elem + 1]
								= element;

			D_RW(c)->last_block->last_elem++;
			D_RW(c)->last_block->occ_elem++;

			index_in_colony = D_RO(c)->occ_elem;
		}

	// create new block
	} else {
		block_constructor(pop, c);

		TOID_ASSIGN(table, D_RO(c)->last_block->table);

		D_RW(table)[0] = element;

		D_RW(c)->last_block->last_elem++;
		D_RW(c)->last_block->occ_elem++;

		index_in_colony = D_RO(c)->occ_elem;
	}


	D_RW(c)->occ_elem++;

	return index_in_colony;
}

/*
 * remove_int -- remove an int element from colony
 */
static int
remove_int(TOID(struct colony) c, size_t index_in_colony)
{
	if (index_in_colony >= D_RW(c)->capacity) {
		printf("element does not exist\n");
		return -1;
	}

	// spr czy nie ma w free

	size_t number_of_block = index_in_colony % D_RW(c)->block_capacity;
	struct block *block_where_is_elem = D_RW(c)->first_block;

	for (size_t i = 1; i < number_of_block; i++) {
		block_where_is_elem = block_where_is_elem->next;
	}

	D_RW(c)->occ_elem--;
	block_where_is_elem->occ_elem--;

	add_to_free_address(c, index_in_colony);

	return 0;
}

int
main(int argc, char *argv[])
{

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

	enum array_types type = get_type(argv[2]);
	int capacity = atoll(argv[3]);

	TOID(struct colony) col;

	if (create_colony(col, type, capacity) == 0)
		printf("create col\n");
	else printf("not create col\n");

	insert_int(col, 4);

	remove_int(col, 1);


	if (delete_colony(col) == 0)
		printf("del col\n");
	else printf("not del col\n");

	pmemobj_close(pop);

	return 0;
}

// realloc changes capacity

// insert_range remove_range

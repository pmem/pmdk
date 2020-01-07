// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016-2019, Intel Corporation */

/*
 * array.c -- example of arrays usage
 */

#include <ex_common.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/stat.h>
#include <libpmemobj.h>

#define TOID_ARRAY(x) TOID(x)
#define COUNT_OF(x) (sizeof(x) / sizeof(x[0]))
#define MAX_BUFFLEN 30
#define MAX_TYPE_NUM 8

POBJ_LAYOUT_BEGIN(array);
POBJ_LAYOUT_TOID(array, struct array_elm);
POBJ_LAYOUT_TOID(array, int);
POBJ_LAYOUT_TOID(array, PMEMoid);
POBJ_LAYOUT_TOID(array, TOID(struct array_elm));
POBJ_LAYOUT_TOID(array, struct array_info);
POBJ_LAYOUT_END(array);

static PMEMobjpool *pop;
enum array_types {
	UNKNOWN_ARRAY_TYPE,
	INT_ARRAY_TYPE,
	PMEMOID_ARRAY_TYPE,
	TOID_ARRAY_TYPE,
	MAX_ARRAY_TYPE
};

struct array_elm {
	int id;
};

struct array_info {
	char name[MAX_BUFFLEN];
	size_t size;
	enum array_types type;
	PMEMoid array;
};

/*
 * print_usage -- print general usage
 */
static void
print_usage(void)
{
	printf("usage: ./array <file-name> "
		"<alloc|realloc|free|print>"
		" <array-name> [<size> [<TOID|PMEMoid|int>]]\n");
}

/*
 * get type -- parse argument given as type of array
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
 * find_aray -- return info about array with proper name
 */
static TOID(struct array_info)
find_array(const char *name)
{
	TOID(struct array_info) info;
	POBJ_FOREACH_TYPE(pop, info) {
		if (strncmp(D_RO(info)->name, name, MAX_BUFFLEN) == 0)
			return info;
	}
	return TOID_NULL(struct array_info);
}

/*
 * elm_constructor -- constructor of array_elm type object
 */
static int
elm_constructor(PMEMobjpool *pop, void *ptr, void *arg)
{
	struct array_elm *obj = (struct array_elm *)ptr;
	int *id = (int *)arg;
	obj->id = *id;
	pmemobj_persist(pop, obj, sizeof(*obj));

	return 0;
}

/*
 * print_int -- print array of int type
 */
static void
print_int(struct array_info *info)
{
	TOID(int) array;
	TOID_ASSIGN(array, info->array);
	for (size_t i = 0; i < info->size; i++)
		printf("%d ", D_RO(array)[i]);
}

/*
 * print_pmemoid -- print array of PMEMoid type
 */
static void
print_pmemoid(struct array_info *info)
{
	TOID(PMEMoid) array;
	TOID(struct array_elm) elm;
	TOID_ASSIGN(array, info->array);
	for (size_t i = 0; i < info->size; i++) {
		TOID_ASSIGN(elm, D_RW(array)[i]);
		printf("%d ", D_RO(elm)->id);
	}
}

/*
 * print_toid -- print array of TOID(struct array_elm) type
 */
static void
print_toid(struct array_info *info)
{
	TOID_ARRAY(TOID(struct array_elm)) array;
	TOID_ASSIGN(array, info->array);
	for (size_t i = 0; i < info->size; i++)
		printf("%d ", D_RO(D_RO(array)[i])->id);
}

typedef void (*fn_print)(struct array_info *info);
static fn_print print_array[] = {NULL, print_int, print_pmemoid, print_toid};

/*
 * free_int -- de-allocate array of int type
 */
static void
free_int(struct array_info *info)
{
	TOID(int) array;
	TOID_ASSIGN(array, info->array);
	/*
	 * When there is persistent array of simple type allocated,
	 * there is enough to de-allocate persistent pointer
	 */
	POBJ_FREE(&array);
}

/*
 * free_pmemoid -- de-allocate array of PMEMoid type
 */
static void
free_pmemoid(struct array_info *info)
{
	TOID(PMEMoid) array;
	TOID_ASSIGN(array, info->array);
	/*
	 * When there is persistent array of persistent pointer type allocated,
	 * there is necessary to de-allocate each element, if they were
	 * allocated earlier
	 */
	for (size_t i = 0; i < info->size; i++)
		pmemobj_free(&D_RW(array)[i]);
	POBJ_FREE(&array);
}

/*
 * free_toid -- de-allocate array of TOID(struct array_elm) type
 */
static void
free_toid(struct array_info *info)
{
	TOID_ARRAY(TOID(struct array_elm)) array;
	TOID_ASSIGN(array, info->array);
	/*
	 * When there is persistent array of persistent pointer type allocated,
	 * there is necessary to de-allocate each element, if they were
	 * allocated earlier
	 */
	for (size_t i = 0; i < info->size; i++)
		POBJ_FREE(&D_RW(array)[i]);
	POBJ_FREE(&array);
}

typedef void (*fn_free)(struct array_info *info);
static fn_free free_array[] = {NULL, free_int, free_pmemoid, free_toid};

/*
 * realloc_int -- reallocate array of int type
 */
static PMEMoid
realloc_int(PMEMoid *info, size_t prev_size, size_t size)
{
	TOID(int) array;
	TOID_ASSIGN(array, *info);

	POBJ_REALLOC(pop, &array, int, size * sizeof(int));
	if (size > prev_size) {
		for (size_t i = prev_size; i < size; i++)
				D_RW(array)[i] = (int)i;
		pmemobj_persist(pop,
			D_RW(array) + prev_size,
			(size - prev_size) * sizeof(*D_RW(array)));
	}
	return array.oid;
}

/*
 * realloc_pmemoid -- reallocate array of PMEMoid type
 */
static PMEMoid
realloc_pmemoid(PMEMoid *info, size_t prev_size, size_t size)
{
	TOID(PMEMoid) array;
	TOID_ASSIGN(array, *info);
	pmemobj_zrealloc(pop, &array.oid, sizeof(PMEMoid) * size,
							TOID_TYPE_NUM(PMEMoid));

	for (size_t i = prev_size; i < size; i++) {
		if (pmemobj_alloc(pop, &D_RW(array)[i],
			sizeof(struct array_elm), TOID_TYPE_NUM(PMEMoid),
							elm_constructor, &i)) {
			fprintf(stderr, "pmemobj_alloc\n");
			assert(0);
		}
	}
	return array.oid;
}

/*
 * realloc_toid -- reallocate array of TOID(struct array_elm) type
 */
static PMEMoid
realloc_toid(PMEMoid *info, size_t prev_size, size_t size)
{
	TOID_ARRAY(TOID(struct array_elm)) array;
	TOID_ASSIGN(array, *info);
	pmemobj_zrealloc(pop, &array.oid,
			sizeof(TOID(struct array_elm)) * size,
			TOID_TYPE_NUM_OF(array));
	for (size_t i = prev_size; i < size; i++) {
		POBJ_NEW(pop, &D_RW(array)[i], struct array_elm,
						elm_constructor, &i);
		if (TOID_IS_NULL(D_RW(array)[i])) {
			fprintf(stderr, "POBJ_ALLOC\n");
			assert(0);
		}
	}
	return array.oid;
}

typedef PMEMoid (*fn_realloc)(PMEMoid *info, size_t prev_size, size_t size);
static fn_realloc realloc_array[] =
	{NULL, realloc_int, realloc_pmemoid, realloc_toid};

/*
 * alloc_int -- allocate array of int type
 */
static PMEMoid
alloc_int(size_t size)
{
	TOID(int) array;
	/*
	 * To allocate persistent array of simple type is enough to allocate
	 * pointer with size equal to number of elements multiplied by size of
	 * user-defined structure.
	 */
	POBJ_ALLOC(pop, &array, int, sizeof(int) * size,
						NULL, NULL);
	if (TOID_IS_NULL(array)) {
		fprintf(stderr, "POBJ_ALLOC\n");
		return OID_NULL;
	}

	for (size_t i = 0; i < size; i++)
		D_RW(array)[i] = (int)i;
	pmemobj_persist(pop, D_RW(array), size * sizeof(*D_RW(array)));
	return array.oid;
}

/*
 * alloc_pmemoid -- allocate array of PMEMoid type
 */
static PMEMoid
alloc_pmemoid(size_t size)
{
	TOID(PMEMoid) array;
	/*
	 * To allocate persistent array of PMEMoid type is necessary to allocate
	 * pointer with size equal to number of elements multiplied by size of
	 * PMEMoid and to allocate each of elements separately.
	 */
	POBJ_ALLOC(pop, &array, PMEMoid, sizeof(PMEMoid) * size,
					NULL, NULL);
	if (TOID_IS_NULL(array)) {
		fprintf(stderr, "POBJ_ALLOC\n");
		return OID_NULL;
	}

	for (size_t i = 0; i < size; i++) {
		if (pmemobj_alloc(pop, &D_RW(array)[i],
			sizeof(struct array_elm),
			TOID_TYPE_NUM(PMEMoid), elm_constructor, &i)) {
			fprintf(stderr, "pmemobj_alloc\n");
		}
	}

	return array.oid;
}

/*
 * alloc_toid -- allocate array of TOID(struct array_elm) type
 */
static PMEMoid
alloc_toid(size_t size)
{
	TOID_ARRAY(TOID(struct array_elm)) array;
	/*
	 * To allocate persistent array of TOID with user-defined structure type
	 * is necessary to allocate pointer with size equal to number of
	 * elements multiplied by size of TOID of proper type and to allocate
	 * each of elements separately.
	 */
	POBJ_ALLOC(pop, &array, TOID(struct array_elm),
			sizeof(TOID(struct array_elm)) * size, NULL, NULL);

	if (TOID_IS_NULL(array)) {
		fprintf(stderr, "POBJ_ALLOC\n");
		return OID_NULL;
	}

	for (size_t i = 0; i < size; i++) {
		POBJ_NEW(pop, &D_RW(array)[i], struct array_elm,
						elm_constructor, &i);
		if (TOID_IS_NULL(D_RW(array)[i])) {
			fprintf(stderr, "POBJ_ALLOC\n");
			assert(0);
		}
	}
	return array.oid;
}

typedef PMEMoid (*fn_alloc)(size_t size);
static fn_alloc alloc_array[] = {NULL, alloc_int, alloc_pmemoid, alloc_toid};

/*
 * do_print -- print values stored by proper array
 */
static void
do_print(int argc, char *argv[])
{
	if (argc != 1) {
		printf("usage: ./array <file-name> print <array-name>\n");
		return;
	}

	TOID(struct array_info) array_info = find_array(argv[0]);
	if (TOID_IS_NULL(array_info)) {
		printf("%s doesn't exist\n", argv[0]);
		return;
	}
	printf("%s:\n", argv[0]);
	print_array[D_RO(array_info)->type](D_RW(array_info));
	printf("\n");
}

/*
 * do_free -- de-allocate proper array and proper TOID of array_info type
 */
static void
do_free(int argc, char *argv[])
{
	if (argc != 1) {
		printf("usage: ./array <file-name> free <array-name>\n");
		return;
	}
	TOID(struct array_info) array_info = find_array(argv[0]);
	if (TOID_IS_NULL(array_info)) {
		printf("%s doesn't exist\n", argv[0]);
		return;
	}
	free_array[D_RO(array_info)->type](D_RW(array_info));
	POBJ_FREE(&array_info);
}

/*
 * do_realloc -- reallocate proper array to given size and update information
 * in array_info structure
 */
static void
do_realloc(int argc, char *argv[])
{
	if (argc != 2) {
		printf("usage: ./array <file-name> realloc"
						" <array-name> <size>\n");
		return;
	}
	size_t size = atoi(argv[1]);
	TOID(struct array_info) array_info = find_array(argv[0]);
	if (TOID_IS_NULL(array_info)) {
		printf("%s doesn't exist\n", argv[0]);
		return;
	}
	struct array_info *info = D_RW(array_info);
	info->array = realloc_array[info->type](&info->array, info->size, size);
	if (OID_IS_NULL(info->array)) {
		if (size != 0)
			printf("POBJ_REALLOC\n");
	}
	info->size = size;
	pmemobj_persist(pop, info, sizeof(*info));
}

/*
 * do_alloc -- allocate persistent array and TOID of array_info type
 * and set it with information about new array
 */
static void
do_alloc(int argc, char *argv[])
{
	if (argc != 3) {
		printf("usage: ./array <file-name> alloc <array-name>"
							"<size> <type>\n");
		return;
	}
	enum array_types type = get_type(argv[2]);
	if (type == UNKNOWN_ARRAY_TYPE)
		return;
	size_t size = atoi(argv[1]);
	TOID(struct array_info) array_info = find_array(argv[0]);
	if (!TOID_IS_NULL(array_info))
		POBJ_FREE(&array_info);
	POBJ_ZNEW(pop, &array_info, struct array_info);
	struct array_info *info = D_RW(array_info);
	strncpy(info->name, argv[0], MAX_BUFFLEN - 1);
	info->name[MAX_BUFFLEN - 1] = '\0';
	info->size = size;
	info->type = type;
	info->array = alloc_array[type](size);
	if (OID_IS_NULL(info->array))
		assert(0);
	pmemobj_persist(pop, info, sizeof(*info));
}

typedef void (*fn_op)(int argc, char *argv[]);
static fn_op operations[] = {do_alloc, do_realloc, do_free, do_print};

int
main(int argc, char *argv[])
{
	if (argc < 3) {
		print_usage();
		return 1;
	}

	const char *path = argv[1];

	pop = NULL;

	if (file_exists(path) != 0) {
		if ((pop = pmemobj_create(path, POBJ_LAYOUT_NAME(array),
			PMEMOBJ_MIN_POOL, CREATE_MODE_RW)) == NULL) {
			printf("failed to create pool\n");
			return 1;
		}
	} else {
		if ((pop = pmemobj_open(path, POBJ_LAYOUT_NAME(array)))
								== NULL) {
			printf("failed to open pool\n");
			return 1;
		}
	}
	const char *option = argv[2];
	argv += 3;
	argc -= 3;
	const char *names[] = {"alloc", "realloc", "free", "print"};
	int i = 0;
	for (; i < COUNT_OF(names) && strcmp(option, names[i]) != 0; i++);

	if (i != COUNT_OF(names))
		operations[i](argc, argv);
	else
		print_usage();
	pmemobj_close(pop);
	return 0;
}

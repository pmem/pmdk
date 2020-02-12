// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2019, Intel Corporation */

/*
 * obj_strdup.c -- unit test for pmemobj_strdup
 */
#include <sys/param.h>
#include <string.h>
#include <wchar.h>

#include "unittest.h"
#include "libpmemobj.h"

#define LAYOUT_NAME "strdup"

TOID_DECLARE(char, 0);
TOID_DECLARE(wchar_t, 1);

enum type_number {
	TYPE_SIMPLE,
	TYPE_NULL,
	TYPE_SIMPLE_ALLOC,
	TYPE_SIMPLE_ALLOC_1,
	TYPE_SIMPLE_ALLOC_2,
	TYPE_NULL_ALLOC,
	TYPE_NULL_ALLOC_1,
};

#define TEST_STR_1	"Test string 1"
#define TEST_STR_2	"Test string 2"
#define TEST_WCS_1	L"Test string 3"
#define TEST_WCS_2	L"Test string 4"
#define TEST_STR_EMPTY ""
#define TEST_WCS_EMPTY L""

/*
 * do_strdup -- duplicate a string to not allocated toid using pmemobj_strdup
 */
static void
do_strdup(PMEMobjpool *pop)
{
	TOID(char) str = TOID_NULL(char);
	TOID(wchar_t) wcs = TOID_NULL(wchar_t);
	pmemobj_strdup(pop, &str.oid, TEST_STR_1, TYPE_SIMPLE);
	pmemobj_wcsdup(pop, &wcs.oid, TEST_WCS_1, TYPE_SIMPLE);
	UT_ASSERT(!TOID_IS_NULL(str));
	UT_ASSERT(!TOID_IS_NULL(wcs));
	UT_ASSERTeq(strcmp(D_RO(str), TEST_STR_1), 0);
	UT_ASSERTeq(wcscmp(D_RO(wcs), TEST_WCS_1), 0);
}

/*
 * do_strdup_null -- duplicate a NULL string to not allocated toid
 */
static void
do_strdup_null(PMEMobjpool *pop)
{
	TOID(char) str = TOID_NULL(char);
	TOID(wchar_t) wcs = TOID_NULL(wchar_t);
	pmemobj_strdup(pop, &str.oid, NULL, TYPE_NULL);
	pmemobj_wcsdup(pop, &wcs.oid, NULL, TYPE_NULL);
	UT_ASSERT(TOID_IS_NULL(str));
	UT_ASSERT(TOID_IS_NULL(wcs));
}

/*
 * do_alloc -- allocate toid and duplicate a string
 */
static TOID(char)
do_alloc(PMEMobjpool *pop, const char *s, unsigned type_num)
{
	TOID(char) str;
	POBJ_ZNEW(pop, &str, char);
	pmemobj_strdup(pop, &str.oid, s, type_num);
	UT_ASSERT(!TOID_IS_NULL(str));
	UT_ASSERTeq(strcmp(D_RO(str), s), 0);
	return str;
}

/*
 * do_wcs_alloc -- allocate toid and duplicate a wide character string
 */
static TOID(wchar_t)
do_wcs_alloc(PMEMobjpool *pop, const wchar_t *s, unsigned type_num)
{
	TOID(wchar_t) str;
	POBJ_ZNEW(pop, &str, wchar_t);
	pmemobj_wcsdup(pop, &str.oid, s, type_num);
	UT_ASSERT(!TOID_IS_NULL(str));
	UT_ASSERTeq(wcscmp(D_RO(str), s), 0);
	return str;
}

/*
 * do_strdup_alloc -- duplicate a string to allocated toid
 */
static void
do_strdup_alloc(PMEMobjpool *pop)
{
	TOID(char) str1 = do_alloc(pop, TEST_STR_1, TYPE_SIMPLE_ALLOC_1);
	TOID(wchar_t) wcs1 = do_wcs_alloc(pop, TEST_WCS_1, TYPE_SIMPLE_ALLOC_1);
	TOID(char) str2 = do_alloc(pop, TEST_STR_2, TYPE_SIMPLE_ALLOC_2);
	TOID(wchar_t) wcs2 = do_wcs_alloc(pop, TEST_WCS_2, TYPE_SIMPLE_ALLOC_2);
	pmemobj_strdup(pop, &str1.oid, D_RO(str2), TYPE_SIMPLE_ALLOC);
	pmemobj_wcsdup(pop, &wcs1.oid, D_RO(wcs2), TYPE_SIMPLE_ALLOC);
	UT_ASSERTeq(strcmp(D_RO(str1), D_RO(str2)), 0);
	UT_ASSERTeq(wcscmp(D_RO(wcs1), D_RO(wcs2)), 0);
}

/*
 * do_strdup_null_alloc -- duplicate a NULL string to allocated toid
 */
static void
do_strdup_null_alloc(PMEMobjpool *pop)
{
	TOID(char) str1 = do_alloc(pop, TEST_STR_1, TYPE_NULL_ALLOC_1);
	TOID(wchar_t) wcs1 = do_wcs_alloc(pop, TEST_WCS_1, TYPE_NULL_ALLOC_1);
	TOID(char) str2 = TOID_NULL(char);
	TOID(wchar_t) wcs2 = TOID_NULL(wchar_t);
	pmemobj_strdup(pop, &str1.oid, D_RO(str2), TYPE_NULL_ALLOC);
	pmemobj_wcsdup(pop, &wcs1.oid, D_RO(wcs2), TYPE_NULL_ALLOC);
	UT_ASSERT(!TOID_IS_NULL(str1));
	UT_ASSERT(!TOID_IS_NULL(wcs1));
}

/*
 * do_strdup_uint64_range -- duplicate string with
 * type number equal to range of unsigned long long int
 */
static void
do_strdup_uint64_range(PMEMobjpool *pop)
{
	TOID(char) str1;
	TOID(char) str2 = do_alloc(pop, TEST_STR_2, TYPE_SIMPLE_ALLOC_1);
	TOID(char) str3;
	TOID(char) str4 = do_alloc(pop, TEST_STR_2, TYPE_SIMPLE_ALLOC_1);
	pmemobj_strdup(pop, &str1.oid, D_RO(str2), UINT64_MAX);
	pmemobj_strdup(pop, &str3.oid, D_RO(str4), UINT64_MAX - 1);
	UT_ASSERTeq(strcmp(D_RO(str1), D_RO(str2)), 0);
	UT_ASSERTeq(strcmp(D_RO(str3), D_RO(str4)), 0);
}

/*
 * do_strdup_alloc_empty_string -- duplicate string to internal container
 * associated with type number equal to range of unsigned long long int
 * and unsigned long long int - 1
 */
static void
do_strdup_alloc_empty_string(PMEMobjpool *pop)
{
	TOID(char) str1 = do_alloc(pop, TEST_STR_1, TYPE_SIMPLE_ALLOC_1);
	TOID(wchar_t) wcs1 = do_wcs_alloc(pop, TEST_WCS_1, TYPE_SIMPLE_ALLOC_1);
	pmemobj_strdup(pop, &str1.oid, TEST_STR_EMPTY, TYPE_SIMPLE_ALLOC);
	pmemobj_wcsdup(pop, &wcs1.oid, TEST_WCS_EMPTY, TYPE_SIMPLE_ALLOC);
	UT_ASSERTeq(strcmp(D_RO(str1), TEST_STR_EMPTY), 0);
	UT_ASSERTeq(wcscmp(D_RO(wcs1), TEST_WCS_EMPTY), 0);
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_strdup");

	if (argc != 2)
		UT_FATAL("usage: %s [file]", argv[0]);

	PMEMobjpool *pop;
	if ((pop = pmemobj_create(argv[1], LAYOUT_NAME, PMEMOBJ_MIN_POOL,
	    S_IWUSR | S_IRUSR)) == NULL)
		UT_FATAL("!pmemobj_create");

	do_strdup(pop);
	do_strdup_null(pop);
	do_strdup_alloc(pop);
	do_strdup_null_alloc(pop);
	do_strdup_uint64_range(pop);
	do_strdup_alloc_empty_string(pop);
	pmemobj_close(pop);

	DONE(NULL);
}

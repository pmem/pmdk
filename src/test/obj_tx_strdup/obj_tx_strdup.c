// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2020, Intel Corporation */

/*
 * obj_tx_strdup.c -- unit test for pmemobj_tx_strdup
 */
#include <sys/param.h>
#include <string.h>
#include <wchar.h>

#include "unittest.h"

#define LAYOUT_NAME "tx_strdup"

TOID_DECLARE(char, 0);
TOID_DECLARE(wchar_t, 1);

enum type_number {
	TYPE_NO_TX,
	TYPE_WCS_NO_TX,
	TYPE_COMMIT,
	TYPE_WCS_COMMIT,
	TYPE_ABORT,
	TYPE_WCS_ABORT,
	TYPE_FREE_COMMIT,
	TYPE_WCS_FREE_COMMIT,
	TYPE_FREE_ABORT,
	TYPE_WCS_FREE_ABORT,
	TYPE_COMMIT_NESTED1,
	TYPE_WCS_COMMIT_NESTED1,
	TYPE_COMMIT_NESTED2,
	TYPE_WCS_COMMIT_NESTED2,
	TYPE_ABORT_NESTED1,
	TYPE_WCS_ABORT_NESTED1,
	TYPE_ABORT_NESTED2,
	TYPE_WCS_ABORT_NESTED2,
	TYPE_ABORT_AFTER_NESTED1,
	TYPE_WCS_ABORT_AFTER_NESTED1,
	TYPE_ABORT_AFTER_NESTED2,
	TYPE_WCS_ABORT_AFTER_NESTED2,
	TYPE_NOFLUSH,
	TYPE_WCS_NOFLUSH,
};

#define TEST_STR_1	"Test string 1"
#define TEST_STR_2	"Test string 2"
#define TEST_WCS_1	L"Test string 3"
#define TEST_WCS_2	L"Test string 4"
#define MAX_FUNC	2

typedef void (*fn_tx_strdup)(TOID(char) *str, const char *s,
						unsigned type_num);
typedef void (*fn_tx_wcsdup)(TOID(wchar_t) *wcs, const wchar_t *s,
						unsigned type_num);

static unsigned counter;

/*
 * tx_strdup -- duplicate a string using pmemobj_tx_strdup
 */
static void
tx_strdup(TOID(char) *str, const char *s, unsigned type_num)
{
	TOID_ASSIGN(*str, pmemobj_tx_strdup(s, type_num));
}

/*
 * tx_wcsdup -- duplicate a string using pmemobj_tx_wcsdup
 */
static void
tx_wcsdup(TOID(wchar_t) *wcs, const wchar_t *s, unsigned type_num)
{
	TOID_ASSIGN(*wcs, pmemobj_tx_wcsdup(s, type_num));
}

/*
 * tx_strdup_macro -- duplicate a string using macro
 */
static void
tx_strdup_macro(TOID(char) *str, const char *s, unsigned type_num)
{
	TOID_ASSIGN(*str, TX_STRDUP(s, type_num));
}

/*
 * tx_wcsdup_macro -- duplicate a wide character string using macro
 */
static void
tx_wcsdup_macro(TOID(wchar_t) *wcs, const wchar_t *s, unsigned type_num)
{
	TOID_ASSIGN(*wcs, TX_WCSDUP(s, type_num));
}

static fn_tx_strdup do_tx_strdup[MAX_FUNC] = {tx_strdup, tx_strdup_macro};
static fn_tx_wcsdup do_tx_wcsdup[MAX_FUNC] = {tx_wcsdup, tx_wcsdup_macro};

/*
 * do_tx_strdup_commit -- duplicate a string and commit the transaction
 */
static void
do_tx_strdup_commit(PMEMobjpool *pop)
{
	TOID(char) str;
	TOID(wchar_t) wcs;
	TX_BEGIN(pop) {
		do_tx_strdup[counter](&str, TEST_STR_1, TYPE_COMMIT);
		do_tx_wcsdup[counter](&wcs, TEST_WCS_1, TYPE_WCS_COMMIT);
		UT_ASSERT(!TOID_IS_NULL(str));
		UT_ASSERT(!TOID_IS_NULL(wcs));
	} TX_ONABORT {
		UT_ASSERT(0);
	} TX_END

	TOID_ASSIGN(str, POBJ_FIRST_TYPE_NUM(pop, TYPE_COMMIT));
	TOID_ASSIGN(wcs, POBJ_FIRST_TYPE_NUM(pop, TYPE_WCS_COMMIT));
	UT_ASSERT(!TOID_IS_NULL(str));
	UT_ASSERTeq(strcmp(TEST_STR_1, D_RO(str)), 0);
	UT_ASSERTeq(wcscmp(TEST_WCS_1, D_RO(wcs)), 0);
}

/*
 * do_tx_strdup_abort -- duplicate a string and abort the transaction
 */
static void
do_tx_strdup_abort(PMEMobjpool *pop)
{
	TOID(char) str;
	TOID(wchar_t) wcs;
	TX_BEGIN(pop) {
		do_tx_strdup[counter](&str, TEST_STR_1, TYPE_ABORT);
		do_tx_wcsdup[counter](&wcs, TEST_WCS_1, TYPE_WCS_ABORT);
		UT_ASSERT(!TOID_IS_NULL(str));
		UT_ASSERT(!TOID_IS_NULL(wcs));
		pmemobj_tx_abort(-1);
	} TX_ONCOMMIT {
		UT_ASSERT(0);
	} TX_END

	TOID_ASSIGN(str, POBJ_FIRST_TYPE_NUM(pop, TYPE_ABORT));
	TOID_ASSIGN(wcs, POBJ_FIRST_TYPE_NUM(pop, TYPE_WCS_ABORT));
	UT_ASSERT(TOID_IS_NULL(str));
	UT_ASSERT(TOID_IS_NULL(wcs));
}

/*
 * do_tx_strdup_null -- duplicate a NULL string to trigger tx abort
 */
static void
do_tx_strdup_null(PMEMobjpool *pop)
{
	TOID(char) str;
	TOID(wchar_t) wcs;
	TX_BEGIN(pop) {
		do_tx_strdup[counter](&str, NULL, TYPE_ABORT);
		do_tx_wcsdup[counter](&wcs, NULL, TYPE_WCS_ABORT);
		UT_ASSERT(0); /* should not get to this point */
	} TX_ONCOMMIT {
		UT_ASSERT(0);
	} TX_END

	TOID_ASSIGN(str, POBJ_FIRST_TYPE_NUM(pop, TYPE_ABORT));
	TOID_ASSIGN(wcs, POBJ_FIRST_TYPE_NUM(pop, TYPE_WCS_ABORT));
	UT_ASSERT(TOID_IS_NULL(str));
	UT_ASSERT(TOID_IS_NULL(wcs));

	TX_BEGIN(pop) {
		pmemobj_tx_xstrdup(NULL, TYPE_ABORT, POBJ_XALLOC_NO_ABORT);
	} TX_ONCOMMIT {
		UT_ASSERTeq(errno, EINVAL);
	} TX_ONABORT {
		UT_ASSERT(0);
	} TX_END

	TX_BEGIN(pop) {
		pmemobj_tx_set_failure_behavior(POBJ_TX_FAILURE_RETURN);
		pmemobj_tx_strdup(NULL, TYPE_ABORT);
	} TX_ONCOMMIT {
		UT_ASSERTeq(errno, EINVAL);
	} TX_ONABORT {
		UT_ASSERT(0);
	} TX_END

	TX_BEGIN(pop) {
		pmemobj_tx_set_failure_behavior(POBJ_TX_FAILURE_RETURN);
		pmemobj_tx_xstrdup(NULL, TYPE_ABORT, 0);
	} TX_ONCOMMIT {
		UT_ASSERTeq(errno, EINVAL);
	} TX_ONABORT {
		UT_ASSERT(0);
	} TX_END
}

/*
 * do_tx_strdup_free_commit -- duplicate a string, free and commit the
 * transaction
 */
static void
do_tx_strdup_free_commit(PMEMobjpool *pop)
{
	TOID(char) str;
	TOID(wchar_t) wcs;
	TX_BEGIN(pop) {
		do_tx_strdup[counter](&str, TEST_STR_1, TYPE_FREE_COMMIT);
		do_tx_wcsdup[counter](&wcs, TEST_WCS_1, TYPE_WCS_FREE_COMMIT);
		UT_ASSERT(!TOID_IS_NULL(str));
		UT_ASSERT(!TOID_IS_NULL(wcs));
		int ret = pmemobj_tx_free(str.oid);
		UT_ASSERTeq(ret, 0);
		ret = pmemobj_tx_free(wcs.oid);
		UT_ASSERTeq(ret, 0);
	} TX_ONABORT {
		UT_ASSERT(0);
	} TX_END

	TOID_ASSIGN(str, POBJ_FIRST_TYPE_NUM(pop, TYPE_FREE_COMMIT));
	TOID_ASSIGN(wcs, POBJ_FIRST_TYPE_NUM(pop, TYPE_WCS_FREE_COMMIT));
	UT_ASSERT(TOID_IS_NULL(str));
	UT_ASSERT(TOID_IS_NULL(wcs));
}

/*
 * do_tx_strdup_free_abort -- duplicate a string, free and abort the
 * transaction
 */
static void
do_tx_strdup_free_abort(PMEMobjpool *pop)
{
	TOID(char) str;
	TOID(wchar_t) wcs;
	TX_BEGIN(pop) {
		do_tx_strdup[counter](&str, TEST_STR_1, TYPE_FREE_ABORT);
		do_tx_wcsdup[counter](&wcs, TEST_WCS_1, TYPE_WCS_FREE_ABORT);
		UT_ASSERT(!TOID_IS_NULL(str));
		UT_ASSERT(!TOID_IS_NULL(wcs));
		int ret = pmemobj_tx_free(str.oid);
		UT_ASSERTeq(ret, 0);
		ret = pmemobj_tx_free(wcs.oid);
		UT_ASSERTeq(ret, 0);
		pmemobj_tx_abort(-1);
	} TX_ONCOMMIT {
		UT_ASSERT(0);
	} TX_END

	TOID_ASSIGN(str, POBJ_FIRST_TYPE_NUM(pop, TYPE_FREE_ABORT));
	TOID_ASSIGN(wcs, POBJ_FIRST_TYPE_NUM(pop, TYPE_WCS_FREE_ABORT));
	UT_ASSERT(TOID_IS_NULL(str));
	UT_ASSERT(TOID_IS_NULL(wcs));
}

/*
 * do_tx_strdup_commit_nested -- duplicate two string  suing nested
 * transaction and commit the transaction
 */
static void
do_tx_strdup_commit_nested(PMEMobjpool *pop)
{
	TOID(char) str1;
	TOID(char) str2;
	TOID(wchar_t) wcs1;
	TOID(wchar_t) wcs2;

	TX_BEGIN(pop) {
		do_tx_strdup[counter](&str1, TEST_STR_1, TYPE_COMMIT_NESTED1);
		do_tx_wcsdup[counter](&wcs1, TEST_WCS_1,
				TYPE_WCS_COMMIT_NESTED1);
		UT_ASSERT(!TOID_IS_NULL(str1));
		UT_ASSERT(!TOID_IS_NULL(wcs1));
		TX_BEGIN(pop) {
			do_tx_strdup[counter](&str2, TEST_STR_2,
						TYPE_COMMIT_NESTED2);
			do_tx_wcsdup[counter](&wcs2, TEST_WCS_2,
						TYPE_WCS_COMMIT_NESTED2);
			UT_ASSERT(!TOID_IS_NULL(str2));
			UT_ASSERT(!TOID_IS_NULL(wcs2));
		} TX_ONABORT {
			UT_ASSERT(0);
		} TX_END
	} TX_ONABORT {
		UT_ASSERT(0);
	} TX_END

	TOID_ASSIGN(str1, POBJ_FIRST_TYPE_NUM(pop, TYPE_COMMIT_NESTED1));
	TOID_ASSIGN(wcs1, POBJ_FIRST_TYPE_NUM(pop, TYPE_WCS_COMMIT_NESTED1));
	UT_ASSERT(!TOID_IS_NULL(str1));
	UT_ASSERT(!TOID_IS_NULL(wcs1));
	UT_ASSERTeq(strcmp(TEST_STR_1, D_RO(str1)), 0);
	UT_ASSERTeq(wcscmp(TEST_WCS_1, D_RO(wcs1)), 0);

	TOID_ASSIGN(str2, POBJ_FIRST_TYPE_NUM(pop, TYPE_COMMIT_NESTED2));
	TOID_ASSIGN(wcs2, POBJ_FIRST_TYPE_NUM(pop, TYPE_WCS_COMMIT_NESTED2));
	UT_ASSERT(!TOID_IS_NULL(str2));
	UT_ASSERT(!TOID_IS_NULL(wcs2));
	UT_ASSERTeq(strcmp(TEST_STR_2, D_RO(str2)), 0);
	UT_ASSERTeq(wcscmp(TEST_WCS_2, D_RO(wcs2)), 0);
}

/*
 * do_tx_strdup_commit_abort -- duplicate two string  suing nested
 * transaction and abort the transaction
 */
static void
do_tx_strdup_abort_nested(PMEMobjpool *pop)
{
	TOID(char) str1;
	TOID(char) str2;
	TOID(wchar_t) wcs1;
	TOID(wchar_t) wcs2;

	TX_BEGIN(pop) {
		do_tx_strdup[counter](&str1, TEST_STR_1, TYPE_ABORT_NESTED1);
		do_tx_wcsdup[counter](&wcs1, TEST_WCS_1,
				TYPE_WCS_ABORT_NESTED1);
		UT_ASSERT(!TOID_IS_NULL(str1));
		UT_ASSERT(!TOID_IS_NULL(wcs1));
		TX_BEGIN(pop) {
			do_tx_strdup[counter](&str2, TEST_STR_2,
						TYPE_ABORT_NESTED2);
			do_tx_wcsdup[counter](&wcs2, TEST_WCS_2,
						TYPE_WCS_ABORT_NESTED2);
			UT_ASSERT(!TOID_IS_NULL(str2));
			UT_ASSERT(!TOID_IS_NULL(wcs2));
			pmemobj_tx_abort(-1);
		} TX_ONCOMMIT {
			UT_ASSERT(0);
		} TX_END
	} TX_ONCOMMIT {
		UT_ASSERT(0);
	} TX_END

	TOID_ASSIGN(str1, POBJ_FIRST_TYPE_NUM(pop, TYPE_ABORT_NESTED1));
	TOID_ASSIGN(wcs1, POBJ_FIRST_TYPE_NUM(pop, TYPE_WCS_ABORT_NESTED1));
	UT_ASSERT(TOID_IS_NULL(str1));
	UT_ASSERT(TOID_IS_NULL(wcs1));

	TOID_ASSIGN(str2, POBJ_FIRST_TYPE_NUM(pop, TYPE_ABORT_NESTED2));
	TOID_ASSIGN(wcs2, POBJ_FIRST_TYPE_NUM(pop, TYPE_WCS_ABORT_NESTED2));
	UT_ASSERT(TOID_IS_NULL(str2));
	UT_ASSERT(TOID_IS_NULL(wcs2));
}

/*
 * do_tx_strdup_commit_abort -- duplicate two string  suing nested
 * transaction and abort after the nested transaction
 */
static void
do_tx_strdup_abort_after_nested(PMEMobjpool *pop)
{
	TOID(char) str1;
	TOID(char) str2;
	TOID(wchar_t) wcs1;
	TOID(wchar_t) wcs2;

	TX_BEGIN(pop) {
		do_tx_strdup[counter](&str1, TEST_STR_1,
						TYPE_ABORT_AFTER_NESTED1);
		do_tx_wcsdup[counter](&wcs1, TEST_WCS_1,
						TYPE_WCS_ABORT_AFTER_NESTED1);
		UT_ASSERT(!TOID_IS_NULL(str1));
		UT_ASSERT(!TOID_IS_NULL(wcs1));
		TX_BEGIN(pop) {
			do_tx_strdup[counter](&str2, TEST_STR_2,
						TYPE_ABORT_AFTER_NESTED2);
			do_tx_wcsdup[counter](&wcs2, TEST_WCS_2,
						TYPE_WCS_ABORT_AFTER_NESTED2);
			UT_ASSERT(!TOID_IS_NULL(str2));
			UT_ASSERT(!TOID_IS_NULL(wcs2));
		} TX_ONABORT {
			UT_ASSERT(0);
		} TX_END

		pmemobj_tx_abort(-1);
	} TX_ONCOMMIT {
		UT_ASSERT(0);
	} TX_END

	TOID_ASSIGN(str1, POBJ_FIRST_TYPE_NUM(pop, TYPE_ABORT_AFTER_NESTED1));
	TOID_ASSIGN(wcs1, POBJ_FIRST_TYPE_NUM(pop,
			TYPE_WCS_ABORT_AFTER_NESTED1));
	UT_ASSERT(TOID_IS_NULL(str1));
	UT_ASSERT(TOID_IS_NULL(wcs1));

	TOID_ASSIGN(str2, POBJ_FIRST_TYPE_NUM(pop, TYPE_ABORT_AFTER_NESTED2));
	TOID_ASSIGN(wcs2, POBJ_FIRST_TYPE_NUM(pop,
			TYPE_WCS_ABORT_AFTER_NESTED2));
	UT_ASSERT(TOID_IS_NULL(str2));
	UT_ASSERT(TOID_IS_NULL(wcs2));
}

/*
 * do_tx_strdup_noflush -- allocates zeroed object
 */
static void
do_tx_strdup_noflush(PMEMobjpool *pop)
{
	TX_BEGIN(pop) {
		errno = 0;
		pmemobj_tx_xstrdup(TEST_STR_1, TYPE_NOFLUSH,
				POBJ_XALLOC_NO_FLUSH);
		pmemobj_tx_xwcsdup(TEST_WCS_1, TYPE_WCS_NOFLUSH,
				POBJ_XALLOC_NO_FLUSH);
	} TX_ONCOMMIT {
		UT_ASSERTeq(errno, 0);
	} TX_ONABORT {
		UT_ASSERT(0);
	} TX_END
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_tx_strdup");

	if (argc != 2)
		UT_FATAL("usage: %s [file]", argv[0]);

	PMEMobjpool *pop;
	if ((pop = pmemobj_create(argv[1], LAYOUT_NAME, PMEMOBJ_MIN_POOL,
	    S_IWUSR | S_IRUSR)) == NULL)
		UT_FATAL("!pmemobj_create");

	for (counter = 0; counter < MAX_FUNC; counter++) {
		do_tx_strdup_commit(pop);
		do_tx_strdup_abort(pop);
		do_tx_strdup_null(pop);
		do_tx_strdup_free_commit(pop);
		do_tx_strdup_free_abort(pop);
		do_tx_strdup_commit_nested(pop);
		do_tx_strdup_abort_nested(pop);
		do_tx_strdup_abort_after_nested(pop);
	}

	do_tx_strdup_noflush(pop);

	pmemobj_close(pop);

	DONE(NULL);
}

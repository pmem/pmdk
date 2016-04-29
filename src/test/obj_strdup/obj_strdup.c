/*
 * Copyright 2015-2016, Intel Corporation
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
 * obj_strdup.c -- unit test for pmemobj_strdup
 */
#include <sys/param.h>
#include <string.h>

#include "unittest.h"
#include "libpmemobj.h"

#define LAYOUT_NAME "strdup"

TOID_DECLARE(char, 0);

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

/*
 * do_strdup -- duplicate a string to not allocated toid using pmemobj_strdup
 */
static void
do_strdup(PMEMobjpool *pop)
{
	TOID(char) str = TOID_NULL(char);
	pmemobj_strdup(pop, &str.oid, TEST_STR_1, TYPE_SIMPLE);
	UT_ASSERT(!TOID_IS_NULL(str));
	UT_ASSERTeq(strcmp(D_RO(str), TEST_STR_1), 0);
}

/*
 * do_strdup_null -- duplicate a NULL string to not allocated toid
 */
static void
do_strdup_null(PMEMobjpool *pop)
{
	TOID(char) str = TOID_NULL(char);
	pmemobj_strdup(pop, &str.oid, NULL, TYPE_NULL);
	UT_ASSERT(TOID_IS_NULL(str));
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
 * do_strdup_alloc -- duplicate a string to allocated toid
 */
static void
do_strdup_alloc(PMEMobjpool *pop)
{
	TOID(char) str1 = do_alloc(pop, TEST_STR_1, TYPE_SIMPLE_ALLOC_1);
	TOID(char) str2 = do_alloc(pop, TEST_STR_2, TYPE_SIMPLE_ALLOC_2);
	pmemobj_strdup(pop, &str1.oid, D_RO(str2), TYPE_SIMPLE_ALLOC);
	UT_ASSERTeq(strcmp(D_RO(str1), D_RO(str2)), 0);
}

/*
 * do_strdup_null_alloc -- duplicate a NULL string to allocated toid
 */
static void
do_strdup_null_alloc(PMEMobjpool *pop)
{
	TOID(char) str1 = do_alloc(pop, TEST_STR_1, TYPE_NULL_ALLOC_1);
	TOID(char) str2 = TOID_NULL(char);
	pmemobj_strdup(pop, &str1.oid, D_RO(str2), TYPE_NULL_ALLOC);
	UT_ASSERT(!TOID_IS_NULL(str1));
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
	pmemobj_close(pop);

	DONE(NULL);
}

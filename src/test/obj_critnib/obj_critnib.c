/*
 * Copyright 2015-2018, Intel Corporation
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
 * obj_critnib.c -- unit test for critnib hash table
 */

#include <errno.h>

#include "unittest.h"
#include "critnib.h"
#include "util.h"
#include "libpmemobj.h"

#define TEST_INSERTS 100
#define TEST_VAL(x) ((void *)((uintptr_t)(x)))

static int Rcounter_malloc;

static void *
__wrap_malloc(size_t size)
{
	switch (util_fetch_and_add32(&Rcounter_malloc, 1)) {
		case 1: /* internal out_err malloc */
		default:
			return malloc(size);
		case 2: /* tab malloc */
		case 0: /* critnib malloc */
			return NULL;
	}
}

static void
test_critnib_new_delete(void)
{
	struct critnib *c = NULL;

	/* critnib malloc fail */
	c = critnib_new();
	UT_ASSERT(c == NULL);

	/* first insert malloc fail */
	c = critnib_new();
	UT_ASSERT(critnib_insert(c, 0, NULL) == ENOMEM);
	critnib_delete(c);

	/* all ok */
	c = critnib_new();
	UT_ASSERT(c != NULL);

	critnib_delete(c);
}

static void
test_insert_get_remove(void)
{
	struct critnib *c = critnib_new();
	UT_ASSERT(c != NULL);

	for (unsigned i = 0; i < TEST_INSERTS; ++i)
		UT_ASSERT(critnib_insert(c, i, TEST_VAL(i)) == 0);

	for (unsigned i = 0; i < TEST_INSERTS; ++i)
		UT_ASSERT(critnib_get(c, i) == TEST_VAL(i));

	for (unsigned i = 0; i < TEST_INSERTS; ++i)
		UT_ASSERT(critnib_remove(c, i) == TEST_VAL(i));

	for (unsigned i = 0; i < TEST_INSERTS; ++i)
		UT_ASSERT(critnib_remove(c, i) == NULL);

	for (unsigned i = 0; i < TEST_INSERTS; ++i)
		UT_ASSERT(critnib_get(c, i) == NULL);

	critnib_delete(c);
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_critnib");

	Malloc = __wrap_malloc;

	test_critnib_new_delete();
	test_insert_get_remove();

	DONE(NULL);
}

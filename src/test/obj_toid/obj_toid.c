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
 * obj_toid.c -- unit test for TOID_VALID, DIRECT_RO, DIRECT_RW macros
 */
#include <sys/param.h>
#include "unittest.h"

#define LAYOUT_NAME "toid"
#define TEST_NUM 5
TOID_DECLARE(struct obj, 0);

struct obj {
	int id;
};

/*
 * do_toid_valid -- validates if type number is equal to object's metadata
 */
static void
do_toid_valid(PMEMobjpool *pop)
{
	TOID(struct obj) obj;
	POBJ_NEW(pop, &obj, struct obj, NULL, NULL);
	UT_ASSERT(!TOID_IS_NULL(obj));

	UT_ASSERT(TOID_VALID(obj));
	POBJ_FREE(&obj);
}

/*
 * do_toid_no_valid -- validates if type number is not equal to
 * object's metadata
 */
static void
do_toid_no_valid(PMEMobjpool *pop)
{
	TOID(struct obj) obj;
	int ret = pmemobj_alloc(pop, &obj.oid, sizeof(struct obj), TEST_NUM,
								NULL, NULL);
	UT_ASSERTeq(ret, 0);
	UT_ASSERT(!TOID_VALID(obj));
	POBJ_FREE(&obj);
}

/*
 * do_direct_simple - checks if DIRECT_RW and DIRECT_RO macros correctly
 * write and read from member of structure represented by TOID
 */
static void
do_direct_simple(PMEMobjpool *pop)
{
	TOID(struct obj) obj;
	POBJ_NEW(pop, &obj, struct obj, NULL, NULL);
	DIRECT_RW(obj)->id = TEST_NUM;
	UT_ASSERTeq(DIRECT_RO(obj)->id, TEST_NUM);
	POBJ_FREE(&obj);
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_toid");

	if (argc != 2)
		UT_FATAL("usage: %s [file]", argv[0]);

	PMEMobjpool *pop;
	if ((pop = pmemobj_create(argv[1], LAYOUT_NAME, PMEMOBJ_MIN_POOL,
	    S_IWUSR | S_IRUSR)) == NULL)
		UT_FATAL("!pmemobj_create");

	do_toid_valid(pop);
	do_toid_no_valid(pop);
	do_direct_simple(pop);

	pmemobj_close(pop);

	DONE(NULL);
}

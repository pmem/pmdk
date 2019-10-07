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

/*
 * obj_badblock.c -- Badblock tests on obj pool
 *
 */

#include <stddef.h>

#include "unittest.h"
#include "libpmemobj.h"

#define LAYOUT_NAME "obj_badblock"
#define TEST_ALLOC_COUNT 2048

struct cargs {
	size_t size;
};

static int
test_constructor(PMEMobjpool *pop, void *addr, void *args)
{
	struct cargs *a = args;
	pmemobj_memset_persist(pop, addr, a->size % 256, a->size);

	return 0;
}

static void
do_create(const char *path)
{
	PMEMobjpool *pop = NULL;
	if ((pop = pmemobj_create(path, LAYOUT_NAME,
			0, S_IWUSR | S_IRUSR)) == NULL)
		UT_FATAL("!pmemobj_create: %s", path);

	PMEMoid *oid = MALLOC(sizeof(PMEMoid) * TEST_ALLOC_COUNT);

	if (pmemobj_alloc(pop, &oid[0], 0, 0, NULL, NULL) == 0)
		UT_FATAL("pmemobj_alloc(0) succeeded");

	struct cargs args = { 1024 * 1024 };
	for (unsigned i = 1; i < TEST_ALLOC_COUNT; ++i) {
		if (pmemobj_alloc(pop, &oid[i], i, 0,
				test_constructor, &args) != 0)
			UT_FATAL("!pmemobj_alloc");
		UT_ASSERT(!OID_IS_NULL(oid[i]));
	}
	pmemobj_close(pop);

	FREE(oid);

	UT_ASSERT(pmemobj_check(path, LAYOUT_NAME) == 1);
}

static void
do_open(const char *path)
{
	PMEMobjpool *pop = pmemobj_open(path, LAYOUT_NAME);
	UT_ASSERT(pop != NULL);
	pmemobj_close(pop);
}

int main(int argc, char **argv) {
	START(argc, argv, "obj_badblock");

	if (argc < 3)
		UT_FATAL("usage: %s file-name, o|c", argv[0]);

	const char *path = argv[1];

	for (int arg = 2; arg < argc; arg++) {
		if (argv[arg][1] != '\0')
			UT_FATAL(
				"op must be c or o (c=clear, o=open)");
		switch (argv[arg][0]) {
		case 'c':
			do_create(path);
			break;
		case 'o':
			do_open(path);
		default:
			UT_FATAL(
				"op must be c or o (c=clear, o=open)");
			break;
		}
	}

	DONE(NULL);
}

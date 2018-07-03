/*
 * Copyright 2018, Intel Corporation
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
 * obj_ctl_debug.c -- tests for the ctl debug namesapce entry points
 */

#include "unittest.h"
#include "../../libpmemobj/obj.h"

#define LAYOUT "obj_ctl_debug"
#define BUFFER_SIZE 128
#define ALLOC_PATTERN 0xAC

static void
test_alloc_pattern(PMEMobjpool *pop)
{
	int ret;
	int pattern;
	PMEMoid oid;

	/* check default pattern */
	ret = pmemobj_ctl_get(pop, "debug.heap.alloc_pattern", &pattern);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(pattern, PALLOC_CTL_DEBUG_NO_PATTERN);

	/* check set pattern */
	pattern = ALLOC_PATTERN;
	ret = pmemobj_ctl_set(pop, "debug.heap.alloc_pattern", &pattern);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(pop->heap.alloc_pattern, pattern);

	/* check alloc with pattern */
	ret = pmemobj_alloc(pop, &oid, BUFFER_SIZE, 0, NULL, NULL);
	UT_ASSERTeq(ret, 0);

	char *buff = pmemobj_direct(oid);
	int i;
	for (i = 0; i < BUFFER_SIZE; i++)
		/* should trigger memcheck error: read uninitialized values */
		UT_ASSERTeq(*(buff + i), (char)pattern);

	pmemobj_free(&oid);
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_ctl_debug");

	if (argc < 2)
		UT_FATAL("usage: %s filename", argv[0]);

	const char *path = argv[1];

	PMEMobjpool *pop;

	if ((pop = pmemobj_create(path, LAYOUT, PMEMOBJ_MIN_POOL,
		S_IWUSR | S_IRUSR)) == NULL)
		UT_FATAL("!pmemobj_open: %s", path);

	test_alloc_pattern(pop);

	pmemobj_close(pop);

	DONE(NULL);
}

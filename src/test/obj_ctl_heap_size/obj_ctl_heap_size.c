/*
 * Copyright 2017, Intel Corporation
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
 * obj_ctl_heap_size.c -- tests for the ctl entry points: heap.size.*
 */

#include "unittest.h"

#define LAYOUT "obj_ctl_heap_size"
#define CUSTOM_GRANULARITY ((1 << 20) * 10)
#define OBJ_SIZE 1024

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_ctl_heap_size");

	if (argc != 3)
		UT_FATAL("usage: %s poolset [w|x]", argv[0]);

	const char *path = argv[1];
	char t = argv[2][0];

	PMEMobjpool *pop;

	if ((pop = pmemobj_open(path, LAYOUT)) == NULL)
		UT_FATAL("!pmemobj_open: %s", path);

	int ret = 0;
	size_t disable_granularity = 0;
	ret = pmemobj_ctl_set(pop, "heap.size.granularity",
		&disable_granularity);
	UT_ASSERTeq(ret, 0);

	/* allocate until OOM */
	while (pmemobj_alloc(pop, NULL, OBJ_SIZE, 0, NULL, NULL) == 0)
		;

	if (t == 'x') {
		ssize_t extend_size = CUSTOM_GRANULARITY;
		ret = pmemobj_ctl_exec(pop, "heap.size.extend", &extend_size);
		UT_ASSERTeq(ret, 0);
	} else if (t == 'w') {
		ssize_t new_granularity = CUSTOM_GRANULARITY;
		ret = pmemobj_ctl_set(pop, "heap.size.granularity",
			&new_granularity);
		UT_ASSERTeq(ret, 0);

		ssize_t curr_granularity;
		ret = pmemobj_ctl_get(pop, "heap.size.granularity",
			&curr_granularity);
		UT_ASSERTeq(ret, 0);
		UT_ASSERTeq(new_granularity, curr_granularity);
	} else {
		UT_ASSERT(0);
	}

	/* should succeed */
	ret = pmemobj_alloc(pop, NULL, OBJ_SIZE, 0, NULL, NULL);
	UT_ASSERTeq(ret, 0);

	pmemobj_close(pop);

	DONE(NULL);
}

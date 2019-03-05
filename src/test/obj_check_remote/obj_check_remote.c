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
 * obj_check_remote.c -- unit tests for pmemobj_check_remote
 */

#include <stddef.h>
#include "unittest.h"
#include "libpmemobj.h"

struct vector {
	int x;
	int y;
	int z;
};

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_check_remote");

	if (argc < 3)
		UT_FATAL("insufficient number of arguments");

	const char *path = argv[1];
	const char *action = argv[2];
	const char *layout = NULL;
	PMEMobjpool *pop = NULL;

	if (strcmp(action, "abort") == 0) {
		pop = pmemobj_open(path, layout);
		if (pop == NULL)
			UT_FATAL("usage: %s filename abort|check", argv[0]);

		PMEMoid root = pmemobj_root(pop, sizeof(struct vector));
		struct vector *vectorp = pmemobj_direct(root);

		TX_BEGIN(pop) {
			pmemobj_tx_add_range(root, 0, sizeof(struct vector));
			vectorp->x = 5;
			vectorp->y = 10;
			vectorp->z = 15;
		} TX_ONABORT {
			UT_ASSERT(0);
		} TX_END

		int *to_modify = &vectorp->x;

		TX_BEGIN(pop) {
			pmemobj_tx_add_range_direct(to_modify, sizeof(int));
			*to_modify = 30;
			pmemobj_persist(pop, to_modify, sizeof(*to_modify));
			abort();
		} TX_END
	} else if (strcmp(action, "check") == 0) {
		int ret = pmemobj_check(path, layout);
		if (ret == 1)
			return 0;
		else
			return ret;
	} else {
		UT_FATAL("%s is not a valid action", action);
	}

	return 0;
}

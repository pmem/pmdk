/*
 * Copyright 2016-2017, Intel Corporation
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
 * obj_ctl_prefault.c -- tests for the ctl entry points: prefault
 */

#include <sys/resource.h>
#include "unittest.h"

#define LAYOUT "obj_ctl_prefault"

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_ctl_prefault");

	if (argc != 4)
		UT_FATAL("usage: %s file-name prefault(0/1/2) open(0/1)",
		argv[0]);

	const char *path = argv[1];
	int prefault = argv[2][0] - '0';
	int open = argv[3][0] - '0';

	PMEMobjpool *pop;
	int ret;
	int arg;
	int arg_read;

	if (prefault == 1) { /* prefault at open */
		arg_read = -1;
		ret = pmemobj_ctl_get(NULL, "prefault.at_open", &arg_read);
		UT_ASSERTeq(ret, 0);
		UT_ASSERTeq(arg_read, 0);

		arg = 1;
		ret = pmemobj_ctl_set(NULL, "prefault.at_open", &arg);
		UT_ASSERTeq(ret, 0);
		UT_ASSERTeq(arg, 1);

		arg_read = -1;
		ret = pmemobj_ctl_get(NULL, "prefault.at_open", &arg_read);
		UT_ASSERTeq(ret, 0);
		UT_ASSERTeq(arg_read, 1);
	} else if (prefault == 2) { /* prefault at create */
		arg_read = -1;
		ret = pmemobj_ctl_get(NULL, "prefault.at_create", &arg_read);
		UT_ASSERTeq(ret, 0);
		UT_ASSERTeq(arg_read, 0);

		arg = 1;
		ret = pmemobj_ctl_set(NULL, "prefault.at_create", &arg);
		UT_ASSERTeq(ret, 0);
		UT_ASSERTeq(arg, 1);

		arg_read = -1;
		ret = pmemobj_ctl_get(NULL, "prefault.at_create", &arg_read);
		UT_ASSERTeq(ret, 0);
		UT_ASSERTeq(arg_read, 1);
	}

	if (open) {
		if ((pop = pmemobj_open(path, LAYOUT)) == NULL)
			UT_FATAL("!pmemobj_open: %s", path);
	} else {
		if ((pop = pmemobj_create(path, LAYOUT, PMEMOBJ_MIN_POOL,
			S_IWUSR | S_IRUSR)) == NULL)
			UT_FATAL("!pmemobj_create: %s", path);
	}

	size_t length = PMEMOBJ_MIN_POOL;
	size_t arr_len = (length + Ut_pagesize - 1) / Ut_pagesize;
	unsigned char *vec = MALLOC(sizeof(*vec) * arr_len);

	ret = mincore(pop, length, vec);
	UT_ASSERTeq(ret, 0);

	size_t resident_pages = 0;
	for (size_t i = 0; i < arr_len; ++i)
		resident_pages += vec[i];

	FREE(vec);

	pmemobj_close(pop);

	UT_OUT("%ld", resident_pages);

	DONE(NULL);
}

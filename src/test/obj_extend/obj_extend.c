/*
 * Copyright 2017-2018, Intel Corporation
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
 * obj_extend.c -- pool extension tests
 *
 */

#include <stddef.h>

#include "unittest.h"

#define ALLOC_SIZE (((1 << 20) * 2) - 16) /* 2 megabytes - 16 bytes (hdr) */
#define RESV_SIZE ((1 << 29) + ((1 << 20) * 8)) /* 512 + 8 megabytes */
#define FRAG 0.9

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_extend");

	if (argc != 2)
		UT_FATAL("usage: %s file-name", argv[0]);

	const char *path = argv[1];

	PMEMobjpool *pop = NULL;

	if ((pop = pmemobj_create(path, "obj_extend",
			0, S_IWUSR | S_IRUSR)) == NULL) {
		UT_ERR("pmemobj_create: %s", pmemobj_errormsg());
		exit(0);
	}

	size_t allocated = 0;
	PMEMoid oid;
	while (pmemobj_alloc(pop, &oid, ALLOC_SIZE, 0, NULL, NULL) == 0) {
		allocated += pmemobj_alloc_usable_size(oid);
	}

	UT_ASSERT(allocated > (RESV_SIZE * FRAG));

	pmemobj_close(pop);

	if ((pop = pmemobj_open(path, "obj_extend")) == NULL)
		UT_FATAL("!pmemobj_open: %s", path);

	pmemobj_close(pop);

	int result = pmemobj_check(path, "obj_extend");
	UT_ASSERTeq(result, 1);

	DONE(NULL);
}

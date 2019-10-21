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
 * obj_defrag.c -- unit test for pmemobj_alloc and pmemobj_zalloc
 */

#include "unittest.h"
#include <limits.h>

#define OBJECT_SIZE 100

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_alloc");

	const char *path = argv[1];
	PMEMobjpool *pop = NULL;

	path = argv[1];

	pop = pmemobj_create(path, POBJ_LAYOUT_NAME(basic),
		PMEMOBJ_MIN_POOL, S_IWUSR | S_IRUSR);
	if (pop == NULL) {
		UT_FATAL("!pmemobj_create: %s", path);
	}

	PMEMoid oid1;
	PMEMoid oid2;
	PMEMoid oid3;
	pmemobj_zalloc(pop, &oid1, OBJECT_SIZE, 0);
	pmemobj_zalloc(pop, &oid2, OBJECT_SIZE, 0);
	pmemobj_zalloc(pop, &oid3, OBJECT_SIZE, 0);
	char *buff = MALLOC(OBJECT_SIZE);
	memset(buff, 0xc, OBJECT_SIZE);

	char *foop = pmemobj_direct(oid3);
	memcpy(foop, buff, OBJECT_SIZE);

	UT_ASSERT(memcmp(foop, buff, OBJECT_SIZE) == 0);
	pmemobj_free(&oid1);

	PMEMoid oid4 = oid3;
	PMEMoid *oids[] = {&oid2, &oid3, &oid4};
	pmemobj_defrag(pop, oids, 3);

	/* the object at higher location should move into the freed oid1 pos */
	UT_ASSERT(oid3.off < oid2.off);
	UT_ASSERT(memcmp(foop, buff, OBJECT_SIZE) == 0);

	FREE(buff);
	pmemobj_close(pop);
	DONE(NULL);
}

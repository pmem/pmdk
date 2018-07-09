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
 * obj_root.c -- unit tests for pmemobj_root
 */

#include "unittest.h"

#define FILE_SIZE ((size_t)0x440000000) /* 17 GB */

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_root");
	if (argc != 2)
		UT_FATAL("usage: obj_root <file>");

	const char *path = argv[1];
	PMEMobjpool *pop = NULL;
	os_stat_t st;

	os_stat(path, &st);
	UT_ASSERTeq(st.st_size, FILE_SIZE);

	if ((pop = pmemobj_create(path, NULL, 0, S_IWUSR | S_IRUSR)) == NULL)
		UT_FATAL("!pmemobj_create: %s", path);

	errno = 0;
	PMEMoid oid = pmemobj_root(pop, 0);
	UT_ASSERT(OID_EQUALS(oid, OID_NULL));
	UT_ASSERTeq(errno, EINVAL);

	oid = pmemobj_root(pop, PMEMOBJ_MAX_ALLOC_SIZE);
	UT_ASSERT(!OID_EQUALS(oid, OID_NULL));

	oid = pmemobj_root(pop, 1);
	UT_ASSERT(!OID_EQUALS(oid, OID_NULL));

	oid = pmemobj_root(pop, 0);
	UT_ASSERT(!OID_EQUALS(oid, OID_NULL));

	errno = 0;
	oid = pmemobj_root(pop, FILE_SIZE);
	UT_ASSERT(OID_EQUALS(oid, OID_NULL));
	UT_ASSERTeq(errno, ENOMEM);

	errno = 0;
	oid = pmemobj_root(pop, SIZE_MAX);
	UT_ASSERT(OID_EQUALS(oid, OID_NULL));
	UT_ASSERTeq(errno, ENOMEM);

	pmemobj_close(pop);

	DONE(NULL);
}

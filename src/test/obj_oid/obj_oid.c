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
 * obj_oid.c -- unit tests for pmemobj_oid
 */

#include "unittest.h"
#include "libpmemobj.h"

#define MEGABYTE (1 << 20)
#define OBJECT_2M (2 * MEGABYTE)

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_oid");
	if (argc != 2)
		UT_FATAL("usage: obj_oid <file>");

	const char *path = argv[1];
	const char *layout = NULL;
	PMEMobjpool *pop = NULL;
	PMEMoid oid = OID_NULL;

	pop = pmemobj_open(path, layout);
	UT_ASSERTeq(pmemobj_alloc(pop, &oid, OBJECT_2M, 0, NULL, NULL), 0);

	/* Valid address */
	UT_ASSERTeq(OID_IS_NULL(oid), 0);
	void *valid_ptr = pmemobj_direct(oid);
	UT_ASSERTeq(OID_EQUALS(oid, pmemobj_oid(valid_ptr)), 1);

	/* Null address */
	UT_ASSERTeq(OID_EQUALS(pmemobj_oid(NULL), OID_NULL), 1);

	/* Address outside the pmemobj pool */
	void *allocated_memory = malloc(sizeof(int));
	UT_ASSERTeq(OID_IS_NULL(pmemobj_oid(allocated_memory)), 1);

	if (!OID_IS_NULL(oid))
		pmemobj_free(&oid);

	DONE(NULL);
}

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
 * obj_defrag.c -- unit test for pmemobj_defrag
 */

#include "unittest.h"
#include <limits.h>

#define OBJECT_SIZE 100

static void
defrag_basic(PMEMobjpool *pop)
{
	int ret;
	PMEMoid oid1;
	PMEMoid oid2;
	PMEMoid oid3;
	ret = pmemobj_zalloc(pop, &oid1, OBJECT_SIZE, 0);
	UT_ASSERTeq(ret, 0);
	ret = pmemobj_zalloc(pop, &oid2, OBJECT_SIZE, 0);
	UT_ASSERTeq(ret, 0);
	ret = pmemobj_zalloc(pop, &oid3, OBJECT_SIZE, 0);
	UT_ASSERTeq(ret, 0);
	char *buff = (char *)MALLOC(OBJECT_SIZE);
	memset(buff, 0xc, OBJECT_SIZE);

	char *foop = (char *)pmemobj_direct(oid3);
	pmemobj_memcpy_persist(pop, foop, buff, OBJECT_SIZE);

	UT_ASSERT(memcmp(foop, buff, OBJECT_SIZE) == 0);
	pmemobj_free(&oid1);

	PMEMoid oid4 = oid3;
	PMEMoid *oids[] = {&oid2, &oid3, &oid4};
	struct pobj_defrag_result result;
	ret = pmemobj_defrag(pop, oids, 3, &result);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(result.total, 2);
	UT_ASSERTeq(result.relocated, 2);

	/* the object at higher location should move into the freed oid1 pos */
	foop = (char *)pmemobj_direct(oid3);
	UT_ASSERT(oid3.off < oid2.off);
	UT_ASSERTeq(oid3.off, oid4.off);
	UT_ASSERT(memcmp(foop, buff, OBJECT_SIZE) == 0);

	pmemobj_free(&oid2);
	pmemobj_free(&oid3);

	FREE(buff);
}

struct test_object
{
	PMEMoid a;
	PMEMoid b;
	PMEMoid c;
};

static void
defrag_nested_pointers(PMEMobjpool *pop)
{
	int ret;
	/*
	 * This is done so that the oids below aren't allocated literally in the
	 * ideal position in the heap (chunk 0, offset 0).
	 */
#define EXTRA_ALLOCS 100
	for (int i = 0; i < EXTRA_ALLOCS; ++i) {
		PMEMoid extra;
		ret = pmemobj_zalloc(pop, &extra, OBJECT_SIZE, 0);
		UT_ASSERTeq(ret, 0);
		pmemobj_free(&extra);
	}
#undef EXTRA_ALLOCS

	PMEMoid oid1;
	PMEMoid oid2;
	PMEMoid oid3;
	ret = pmemobj_zalloc(pop, &oid1, OBJECT_SIZE, 0);
	UT_ASSERTeq(ret, 0);
	ret = pmemobj_zalloc(pop, &oid2, OBJECT_SIZE, 0);
	UT_ASSERTeq(ret, 0);
	ret = pmemobj_zalloc(pop, &oid3, OBJECT_SIZE, 0);
	UT_ASSERTeq(ret, 0);

	struct test_object *oid1p = (struct test_object *)pmemobj_direct(oid1);
	struct test_object *oid2p = (struct test_object *)pmemobj_direct(oid2);
	struct test_object *oid3p = (struct test_object *)pmemobj_direct(oid3);

	oid1p->a = OID_NULL;
	oid1p->b = oid2;
	oid1p->c = oid1;
	pmemobj_persist(pop, oid1p, sizeof(*oid1p));

	oid2p->a = oid1;
	oid2p->b = OID_NULL;
	oid2p->c = oid3;
	pmemobj_persist(pop, oid2p, sizeof(*oid2p));

	oid3p->a = oid2;
	oid3p->b = oid2;
	oid3p->c = oid1;
	pmemobj_persist(pop, oid3p, sizeof(*oid3p));

#define OID_PTRS 12
#define EXTRA_OID_PTRS 60
#define OIDS_ALL (EXTRA_OID_PTRS + OID_PTRS)
	PMEMoid **oids = (PMEMoid **)MALLOC(sizeof(PMEMoid *) * OIDS_ALL);
	PMEMoid *oid3pprs = (PMEMoid *)MALLOC(sizeof(PMEMoid) * EXTRA_OID_PTRS);
	int i;
	for (i = 0; i < EXTRA_OID_PTRS; ++i) {
		oid3pprs[i] = oid3;
		oids[i] = &oid3pprs[i];
	}

	oids[i + 0] = &oid1;
	oids[i + 1] = &oid2;
	oids[i + 2] = &oid3;
	oids[i + 3] = &oid1p->a;
	oids[i + 4] = &oid1p->b;
	oids[i + 5] = &oid1p->c;
	oids[i + 6] = &oid2p->a;
	oids[i + 7] = &oid2p->b;
	oids[i + 8] = &oid2p->c;
	oids[i + 9] = &oid3p->a;
	oids[i + 10] = &oid3p->b;
	oids[i + 11] = &oid3p->c;

	struct pobj_defrag_result result;
	ret = pmemobj_defrag(pop, oids, OIDS_ALL, &result);
	UT_ASSERTeq(result.total, 3);
	UT_ASSERTeq(result.relocated, 3);
	UT_ASSERTeq(ret, 0);

	oid1p = (struct test_object *)pmemobj_direct(oid1);
	oid2p = (struct test_object *)pmemobj_direct(oid2);
	oid3p = (struct test_object *)pmemobj_direct(oid3);

	for (int i = 0; i < EXTRA_OID_PTRS; ++i) {
		UT_ASSERTeq(oid3pprs[i].off, oid3.off);
	}

	UT_ASSERTeq(oid1p->a.off, 0);
	UT_ASSERTeq(oid1p->b.off, oid2.off);
	UT_ASSERTeq(oid1p->c.off, oid1.off);

	UT_ASSERTeq(oid2p->a.off, oid1.off);
	UT_ASSERTeq(oid2p->b.off, 0);
	UT_ASSERTeq(oid2p->c.off, oid3.off);

	UT_ASSERTeq(oid3p->a.off, oid2.off);
	UT_ASSERTeq(oid3p->b.off, oid2.off);
	UT_ASSERTeq(oid3p->c.off, oid1.off);

	pmemobj_free(&oid1);
	pmemobj_free(&oid2);
	pmemobj_free(&oid3);

	FREE(oids);
	FREE(oid3pprs);
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_defrag");

	const char *path = argv[1];
	PMEMobjpool *pop = NULL;

	pop = pmemobj_create(path, POBJ_LAYOUT_NAME(basic),
		PMEMOBJ_MIN_POOL * 2, S_IWUSR | S_IRUSR);
	if (pop == NULL)
		UT_FATAL("!pmemobj_create: %s", path);

	defrag_basic(pop);
	defrag_nested_pointers(pop);

	pmemobj_close(pop);

	DONE(NULL);
}

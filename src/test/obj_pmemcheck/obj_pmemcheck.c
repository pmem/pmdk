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

#include "unittest.h"
#include "valgrind_internal.h"

struct foo {
	PMEMmutex bar;
};

static void
test_mutex_pmem_mapping_register(PMEMobjpool *pop)
{
	PMEMoid foo;
	int ret = pmemobj_alloc(pop, &foo, sizeof(struct foo), 0, NULL, NULL);
	UT_ASSERTeq(ret, 0);
	UT_ASSERT(!OID_IS_NULL(foo));
	struct foo *foop = pmemobj_direct(foo);
	ret = pmemobj_mutex_lock(pop, &foop->bar);
	/* foo->bar has been removed from pmem mappings collection */
	VALGRIND_PRINT_PMEM_MAPPINGS;

	UT_ASSERTeq(ret, 0);
	ret = pmemobj_mutex_unlock(pop, &foop->bar);
	UT_ASSERTeq(ret, 0);
	pmemobj_free(&foo);
	/* the entire foo object has been re-registered as pmem mapping */
	VALGRIND_PRINT_PMEM_MAPPINGS;
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_pmemcheck");

	if (argc != 2)
		UT_FATAL("usage: %s [file]", argv[0]);

	PMEMobjpool *pop;
	if ((pop = pmemobj_create(argv[1], "pmemcheck", PMEMOBJ_MIN_POOL,
	    S_IWUSR | S_IRUSR)) == NULL)
		UT_FATAL("!pmemobj_create");

	test_mutex_pmem_mapping_register(pop);

	pmemobj_close(pop);

	DONE(NULL);
}

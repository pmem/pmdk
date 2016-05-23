/*
 * Copyright 2015-2016, Intel Corporation
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
 * obj_pvector.c -- unit test for pvector
 */
#include <stdint.h>
#include <pthread.h>

#include "libpmemobj.h"
#include "pvector.h"
#include "unittest.h"

#define PVECTOR_INSERT_VALUES 100000

struct test_root {
	struct pvector vec;
};

static void
vec_zero_entry(PMEMobjpool *pop, uint64_t *entry)
{
	*entry = 0;
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_pvector");
	if (argc != 2)
		UT_FATAL("usage: %s [file]", argv[0]);

	const char *path = argv[1];

	PMEMobjpool *pop;
	if ((pop = pmemobj_create(path, "obj_pvector",
			PMEMOBJ_MIN_POOL, S_IWUSR | S_IRUSR)) == NULL)
		UT_FATAL("!pmemobj_create: %s", path);

	PMEMoid root = pmemobj_root(pop, sizeof(struct test_root));
	struct test_root *r = pmemobj_direct(root);
	UT_ASSERTne(r, NULL);

	struct pvector_context *ctx = pvector_init(pop, &r->vec);

	uint64_t *val = pvector_push_back(ctx);
	*val = 5;

	val = pvector_push_back(ctx);
	*val = 10;

	val = pvector_push_back(ctx);
	*val = 15;

	uint64_t v;

	int n = 0;
	for (v = pvector_first(ctx); v != 0; v = pvector_next(ctx)) {
		if (n == 0)
			UT_ASSERTeq(v, 5);
		if (n == 1)
			UT_ASSERTeq(v, 10);
		if (n == 2)
			UT_ASSERTeq(v, 15);
		if (n == 3)
			UT_ASSERT(0);

		n++;
	}

	uint64_t removed = pvector_pop_back(ctx, NULL);
	UT_ASSERTeq(removed, 15);

	n = 0;
	for (v = pvector_first(ctx); v != 0; v = pvector_next(ctx)) {
		if (n == 0)
			UT_ASSERTeq(v, 5);
		if (n == 1)
			UT_ASSERTeq(v, 10);
		if (n == 3)
			UT_ASSERT(0);
		n++;
	}

	while (pvector_pop_back(ctx, vec_zero_entry) != 0)
		;

	pvector_delete(ctx);

	ctx = pvector_init(pop, &r->vec);
	for (int i = 0; i < PVECTOR_INSERT_VALUES; ++i) {
		val = pvector_push_back(ctx);
		UT_ASSERTne(val, NULL);
		*val = i;
	}

	n = 0;
	for (v = pvector_first(ctx); v != 0; v = pvector_next(ctx)) {
		UT_ASSERTeq(v, n);
		n++;
	}

	n = 0;
	for (int i = PVECTOR_INSERT_VALUES - 1; i >= 0; --i) {
		v = pvector_pop_back(ctx, NULL);
		UT_ASSERTeq(v, i);
	}

	UT_ASSERTeq(pvector_first(ctx), 0);

	pvector_delete(ctx);

	DONE(NULL);
}

// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2018, Intel Corporation */

/*
 * util_vecq.c -- unit test for vecq implementation
 */

#include "unittest.h"
#include "vecq.h"

struct test {
	int foo;
	int bar;
};

static void
vecq_test()
{
	VECQ(testvec, struct test) v;
	VECQ_INIT(&v);

	struct test t = {5, 10};
	struct test t2 = {10, 15};

	int ret;
	ret = VECQ_ENQUEUE(&v, t);
	UT_ASSERTeq(ret, 0);

	ret = VECQ_ENQUEUE(&v, t2);
	UT_ASSERTeq(ret, 0);

	struct test res = VECQ_FRONT(&v);
	UT_ASSERTeq(res.bar, t.bar);

	size_t s = VECQ_SIZE(&v);
	UT_ASSERTeq(s, 2);

	size_t c = VECQ_CAPACITY(&v);
	UT_ASSERTeq(c, 64);

	res = VECQ_DEQUEUE(&v);
	UT_ASSERTeq(res.bar, t.bar);
	res = VECQ_DEQUEUE(&v);
	UT_ASSERTeq(res.bar, t2.bar);

	VECQ_DELETE(&v);
}

static void
vecq_test_grow()
{
	VECQ(testvec, int) v;
	VECQ_INIT(&v);

	for (int j = 0; j < 100; ++j) {
		int n = j * 100;
		for (int i = 1; i < n; ++i) {
			int ret = VECQ_ENQUEUE(&v, i);
			UT_ASSERTeq(ret, 0);
		}

		for (int i = 1; i < n; ++i) {
			int res = VECQ_DEQUEUE(&v);
			UT_ASSERTeq(res, i);
		}
	}

	VECQ_DELETE(&v);
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "util_vecq");

	vecq_test();
	vecq_test_grow();

	DONE(NULL);
}

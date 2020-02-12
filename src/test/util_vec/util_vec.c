// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2017-2019, Intel Corporation */

/*
 * util_vec.c -- unit test for vec implementation
 */

#include "unittest.h"
#include "vec.h"

struct test {
	int foo;
	int bar;
};

static void
vec_test()
{
	VEC(testvec, struct test) v = VEC_INITIALIZER;

	UT_ASSERTeq(VEC_SIZE(&v), 0);

	struct test t = {1, 2};
	struct test t2 = {3, 4};

	VEC_PUSH_BACK(&v, t);
	VEC_PUSH_BACK(&v, t2);

	UT_ASSERTeq(VEC_ARR(&v)[0].foo, 1);
	UT_ASSERTeq(VEC_GET(&v, 1)->foo, 3);

	UT_ASSERTeq(VEC_SIZE(&v), 2);

	int n = 0;
	VEC_FOREACH(t, &v) {
		switch (n) {
		case 0:
			UT_ASSERTeq(t.foo, 1);
			UT_ASSERTeq(t.bar, 2);
			break;
		case 1:
			UT_ASSERTeq(t.foo, 3);
			UT_ASSERTeq(t.bar, 4);
			break;
		}
		n++;
	}
	UT_ASSERTeq(n, 2);
	UT_ASSERTeq(VEC_SIZE(&v), n);

	VEC_POP_BACK(&v);

	n = 0;
	VEC_FOREACH(t, &v) {
		UT_ASSERTeq(t.foo, 1);
		UT_ASSERTeq(t.bar, 2);
		n++;
	}
	UT_ASSERTeq(n, 1);
	UT_ASSERTeq(VEC_SIZE(&v), n);

	VEC_CLEAR(&v);
	UT_ASSERTeq(VEC_SIZE(&v), 0);

	VEC_DELETE(&v);
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "util_vec");

	vec_test();

	DONE(NULL);
}

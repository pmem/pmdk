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
 * util_vec.c -- unit test for vec implementation
 */

#include "unittest.h"

#define Realloc REALLOC

#include "vec.h"

#define Free FREE

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

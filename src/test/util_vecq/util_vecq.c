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

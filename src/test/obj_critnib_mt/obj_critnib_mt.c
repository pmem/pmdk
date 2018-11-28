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
 * obj_critnib_mt.c -- multithreaded unit test for critnib
 */

#include <errno.h>

#include "critnib.h"
#include "libpmemobj.h"
#include "os_thread.h"
#include "unittest.h"
#include "util.h"


#define MAXTHREADS 65536

static int nthreads, nrthreads, nwthreads;

static uint64_t rnd_r64(unsigned *seedp, void *thid)
{
	/* stick thid in the middle */
	uint64_t r = os_rand_r(seedp);
	r = (r & 0xffff) | (r & 0xffff0000) << 32;
	r |= ((uint64_t)thid) << 16;
	return r;
}

static uint64_t rnd16()
{
	return rand() & 0xffff;
}

static uint64_t rnd64()
{
	return rnd16() << 48 | rnd16() << 32 | rnd16() << 16 | rnd16();
}

static int done = 0;
/* 1000 random numbers, shared between threads. */
static uint64_t the1000[1000];
static struct critnib *c;

#define K 0xdeadbeefcafebabe

static void *thread_read1(void *arg)
{
	uint64_t count = 0;
	while (!done) {
		UT_ASSERTeq(critnib_get(c, K), (void *)K);
		count++;
	}
	return (void *)count;
}

static void *thread_read1000(void *arg)
{
	uint64_t count = 0;
	int i = 0;
	while (!done) {
		if (++i == 1000)
			i = 0;
		uint64_t v = the1000[i];
		UT_ASSERTeq(critnib_get(c, v), (void *)v);
		count++;
	}
	return (void *)count;
}

static void *thread_write1000(void *arg)
{
	unsigned seed = (unsigned)(uint64_t)arg;
	uint64_t w1000[1000];
	for (int i = 0; i < ARRAY_SIZE(w1000); i++)
		w1000[i] = rnd_r64(&seed, arg);

	uint64_t count = 0;
	int i = 0;
	while (!done) {
		if (++i == 1000)
			i = 0;
		uint64_t v = w1000[i];
		critnib_insert(c, v, (void *)v);
		uint64_t r = (uint64_t)critnib_remove(c, v);
		UT_ASSERTeq(v, r);
		count++;
	}
	return (void *)count;
}

static void *thread_read_write_remove(void *arg)
{
	unsigned seed = (unsigned)(uint64_t)arg;
	uint64_t count = 0;
	while (!done) {
		uint64_t r, v = rnd_r64(&seed, arg);
		critnib_insert(c, v, (void *)v);
		r = (uint64_t)critnib_get(c, v);
		UT_ASSERTeq(r, v);
		r = (uint64_t)critnib_remove(c, v);
		UT_ASSERTeq(r, v);
		count++;
	}
	return (void *)count;
}

static uint64_t revbits(uint64_t x)
{
	uint64_t y = 0, a = 1, b = 0x8000000000000000;
	for (; b; a <<= 1, b >>= 1) {
		if (x & a)
			y |= b;
	}
	return y;
}

static void *thread_le1(void *arg)
{
	uint64_t count = 0;
	while (!done) {
		uint64_t y = revbits(count);
		if (y < K)
			UT_ASSERTeq(critnib_find_le(c, y), NULL);
		else
			UT_ASSERTeq(critnib_find_le(c, y), (void *)K);
		count++;
	}
	return (void *)count;
}

static void *thread_le1000(void *arg)
{
	uint64_t count = 0;
	while (!done) {
		uint64_t y = revbits(count);
		critnib_find_le(c, y);
		count++;
	}
	return (void *)count;
}

typedef void *(*thread_func_t)(void *);

static void test(int spreload, int rpreload, thread_func_t rthread,
	thread_func_t wthread)
{
	c = critnib_new();
	if (spreload >= 1)
		critnib_insert(c, K, (void *)K);
	if (spreload >= 2)
		critnib_insert(c, 1, (void *)1);
	for (int i = spreload; i < rpreload; i++)
		critnib_insert(c, the1000[i], (void *)the1000[i]);

	os_thread_t th[MAXTHREADS], wr[MAXTHREADS];
	int ntr = wthread ? nrthreads : nthreads;
	int ntw = wthread ? nwthreads : 0;
	done = 0;
	for (int i = 0; i < ntr; i++) {
		UT_ASSERT(!os_thread_create(&th[i], 0, rthread,
			(void *)(uint64_t)i));
	}
	for (int i = 0; i < ntw; i++) {
		UT_ASSERT(!os_thread_create(&wr[i], 0, wthread,
			(void *)(uint64_t)i));
	}
#ifdef _WIN32
	Sleep(1000);
#else
	sleep(1);
#endif
	done = 1;

	for (int i = 0; i < ntr; i++) {
		void *retval;
		UT_ASSERT(!os_thread_join(&th[i], &retval));
	}
	for (int i = 0; i < ntw; i++) {
		void *retval;
		UT_ASSERT(!os_thread_join(&wr[i], &retval));
	}

	critnib_delete(c);
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_critnib_mt");

	for (int i = 0; i < ARRAY_SIZE(the1000); i++)
		the1000[i] = rnd64();

	nthreads = sysconf(_SC_NPROCESSORS_ONLN);
	if (!nthreads)
		nthreads = 8;
	nwthreads = nthreads / 2;
	if (!nwthreads)
		nwthreads = 1;
	nrthreads = nthreads - nwthreads;
	if (!nrthreads)
		nrthreads = 1;

	test(1, 0, thread_read1, NULL);
	test(2, 0, thread_read1, NULL);
	test(1, 1000, thread_read1, NULL);
	test(0, 1000, thread_read1000, NULL);
	test(1, 0, thread_read1, thread_write1000);
	test(0, 1000, thread_read1000, thread_write1000);
	test(0, 0, thread_read_write_remove, NULL);
	test(1, 0, thread_le1, NULL);
	test(0, 1000, thread_le1000, NULL);

	DONE(NULL);
}

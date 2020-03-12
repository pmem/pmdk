// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2018-2020, Intel Corporation */

/*
 * obj_critnib_mt.c -- multithreaded unit test for critnib
 */

#include <errno.h>

#include "critnib.h"
#include "rand.h"
#include "os_thread.h"
#include "unittest.h"
#include "util.h"
#include "valgrind_internal.h"

#define NITER_FAST 200000000
#define NITER_MID   20000000
#define NITER_SLOW   2000000

#define MAXTHREADS 4096

static int nthreads;  /* number of threads */
static int nrthreads; /* in mixed tests, read threads */
static int nwthreads; /* ... and write threads */

static uint64_t
rnd_thid_r64(rng_t *seedp, uint16_t thid)
{
	/*
	 * Stick arg (thread index) onto bits 16..31, to make it impossible for
	 * two worker threads to write the same value, while keeping both ends
	 * pseudo-random.
	 */
	uint64_t r = rnd64_r(seedp);
	r &= ~0xffff0000ULL;
	r |= ((uint64_t)thid) << 16;
	return r;
}

static uint64_t
helgrind_count(uint64_t x)
{
	/* Convert total number of ops to per-thread. */
	x /= (unsigned)nthreads;
	/*
	 * Reduce iteration count when running on foogrind, by a factor of 64.
	 * Multiple instances of foogrind cause exponential slowdown, so handle
	 * that as well (not that it's very useful for us...).
	 */
	return x >> (6 * On_valgrind);
}

/* 1024 random numbers, shared between threads. */
static uint64_t the1024[1024];
static struct critnib *c;

#define K 0xdeadbeefcafebabe

static void *
thread_read1(void *arg)
{
	uint64_t niter = helgrind_count(NITER_FAST);

	for (uint64_t count = 0; count < niter; count++)
		UT_ASSERTeq(critnib_get(c, K), (void *)K);

	return NULL;
}

static void *
thread_read1024(void *arg)
{
	uint64_t niter = helgrind_count(NITER_FAST);

	for (uint64_t count = 0; count < niter; count++) {
		uint64_t v = the1024[count % ARRAY_SIZE(the1024)];
		UT_ASSERTeq(critnib_get(c, v), (void *)v);
	}

	return NULL;
}

static void *
thread_write1024(void *arg)
{
	rng_t rng;
	randomize_r(&rng, (uintptr_t)arg);
	uint64_t w1024[1024];

	for (int i = 0; i < ARRAY_SIZE(w1024); i++)
		w1024[i] = rnd_thid_r64(&rng, (uint16_t)(uintptr_t)arg);

	uint64_t niter = helgrind_count(NITER_SLOW);

	for (uint64_t count = 0; count < niter; count++) {
		uint64_t v = w1024[count % ARRAY_SIZE(w1024)];
		critnib_insert(c, v, (void *)v);
		uint64_t r = (uint64_t)critnib_remove(c, v);
		UT_ASSERTeq(v, r);
	}

	return NULL;
}

static void *
thread_read_write_remove(void *arg)
{
	rng_t rng;
	randomize_r(&rng, (uintptr_t)arg);
	uint64_t niter = helgrind_count(NITER_SLOW);

	for (uint64_t count = 0; count < niter; count++) {
		uint64_t r, v = rnd_thid_r64(&rng, (uint16_t)(uintptr_t)arg);
		critnib_insert(c, v, (void *)v);
		r = (uint64_t)critnib_get(c, v);
		UT_ASSERTeq(r, v);
		r = (uint64_t)critnib_remove(c, v);
		UT_ASSERTeq(r, v);
	}

	return NULL;
}

/*
 * Reverse bits in a number: 1234 -> 4321 (swap _bit_ endianness).
 *
 * Doing this on successive numbers produces a van der Corput sequence,
 * which covers the space nicely (relevant for <= tests).
 */
static uint64_t
revbits(uint64_t x)
{
	uint64_t y = 0;
	uint64_t a = 1;
	uint64_t b = 0x8000000000000000;

	for (; b; a <<= 1, b >>= 1) {
		if (x & a)
			y |= b;
	}

	return y;
}

static void *
thread_le1(void *arg)
{
	uint64_t niter = helgrind_count(NITER_MID);

	for (uint64_t count = 0; count < niter; count++) {
		uint64_t y = revbits(count);
		if (y < K)
			UT_ASSERTeq(critnib_find_le(c, y), NULL);
		else
			UT_ASSERTeq(critnib_find_le(c, y), (void *)K);
	}

	return NULL;
}

static void *
thread_le1024(void *arg)
{
	uint64_t niter = helgrind_count(NITER_MID);

	for (uint64_t count = 0; count < niter; count++) {
		uint64_t y = revbits(count);
		critnib_find_le(c, y);
	}

	return NULL;
}

typedef void *(*thread_func_t)(void *);

/*
 * Before starting the threads, we add "fixed_preload" of static values
 * (K and 1), or "random_preload" of random numbers.  Can't have both.
 */
static void
test(int fixed_preload, int random_preload, thread_func_t rthread,
	thread_func_t wthread)
{
	c = critnib_new();

	if (fixed_preload >= 1)
		critnib_insert(c, K, (void *)K);

	if (fixed_preload >= 2)
		critnib_insert(c, 1, (void *)1);

	for (int i = 0; i < random_preload; i++)
		critnib_insert(c, the1024[i], (void *)the1024[i]);

	os_thread_t th[MAXTHREADS], wr[MAXTHREADS];
	int ntr = wthread ? nrthreads : nthreads;
	int ntw = wthread ? nwthreads : 0;

	for (int i = 0; i < ntr; i++)
		THREAD_CREATE(&th[i], 0, rthread, (void *)(uint64_t)i);

	for (int i = 0; i < ntw; i++)
		THREAD_CREATE(&wr[i], 0, wthread, (void *)(uint64_t)i);

	/* The threads work here... */

	for (int i = 0; i < ntr; i++) {
		void *retval;
		THREAD_JOIN(&th[i], &retval);
	}

	for (int i = 0; i < ntw; i++) {
		void *retval;
		THREAD_JOIN(&wr[i], &retval);
	}

	critnib_delete(c);
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_critnib_mt");

	util_init();
	randomize(1); /* use a fixed reproducible seed */

	for (int i = 0; i < ARRAY_SIZE(the1024); i++)
		the1024[i] = rnd64();

	nthreads = sysconf(_SC_NPROCESSORS_ONLN);
	if (nthreads > MAXTHREADS)
		nthreads = MAXTHREADS;
	if (!nthreads)
		nthreads = 8;

	nwthreads = nthreads / 2;
	if (!nwthreads)
		nwthreads = 1;

	nrthreads = nthreads - nwthreads;
	if (!nrthreads)
		nrthreads = 1;

	test(1, 0, thread_read1, thread_write1024);
	test(0, 1024, thread_read1024, thread_write1024);
	test(0, 0, thread_read_write_remove, NULL);
	test(1, 0, thread_le1, NULL);
	test(0, 1024, thread_le1024, NULL);

	DONE(NULL);
}

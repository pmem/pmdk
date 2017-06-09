/*
 * Copyright 2017, Intel Corporation
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
 * obj_pmalloc_mt.c -- multithreaded test of allocator
 */
#include <stdint.h>

#include "unittest.h"

#define RRAND(seed, max, min) (os_rand_r(&seed) % ((max) - (min)) + (min))

static ssize_t object_size;
static int nobjects;
static int iterations = 1000000;
static unsigned seed;

static void *
test_worker(void *arg)
{
	PMEMobjpool *pop = arg;

	PMEMoid *objects = ZALLOC(sizeof(PMEMoid) * nobjects);
	int fill = 0;

	int ret;
	for (int i = 0; i < iterations; ++i) {
		int fill_ratio = ((double)fill / nobjects) * 100;
		int pos = RRAND(seed, nobjects, 0);
		size_t size = RRAND(seed, object_size, 64);
		if (RRAND(seed, 100, 0) < fill_ratio) {
			if (!OID_IS_NULL(objects[pos])) {
				pmemobj_free(&objects[pos]);
				objects[pos] = OID_NULL;
				fill--;
			}
		} else {
			if (OID_IS_NULL(objects[pos])) {
				ret = pmemobj_alloc(pop, &objects[pos],
					size, 0, NULL, NULL);
				UT_ASSERTeq(ret, 0);
				fill++;
			}
		}
	}

	FREE(objects);

	return NULL;
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_pmalloc_rand_mt");

	if (argc < 5 && argc > 7)
		UT_FATAL("usage: %s [file] "
			"[threads #] [objects #] [object size] "
			"[iterations (def: 1000000)] [seed (def: time)]",
			argv[0]);

	int nthreads = atoi(argv[2]);
	nobjects = atoi(argv[3]);
	object_size = atoi(argv[4]);
	if (argc > 5)
		iterations = atoi(argv[5]);
	if (argc > 6)
		seed = (unsigned)atoi(argv[6]);
	else
		seed = time(NULL);

	PMEMobjpool *pop;

	if (os_access(argv[1], F_OK) != 0) {
		pop = pmemobj_create(argv[1], "TEST",
		(PMEMOBJ_MIN_POOL) + (nthreads * nobjects * object_size),
		0666);
	} else {
		if ((pop = pmemobj_open(argv[1], "TEST")) == NULL) {
			printf("failed to open pool\n");
			return 1;
		}
	}

	if (pop == NULL)
		UT_FATAL("!pmemobj_create");

	os_thread_t *threads = MALLOC(sizeof(os_thread_t) * nthreads);

	for (int i = 0; i < nthreads; ++i) {
		PTHREAD_CREATE(&threads[i], NULL, test_worker, pop);
	}

	for (int i = 0; i < nthreads; ++i) {
		PTHREAD_JOIN(threads[i], NULL);
	}

	FREE(threads);

	pmemobj_close(pop);

	DONE(NULL);
}

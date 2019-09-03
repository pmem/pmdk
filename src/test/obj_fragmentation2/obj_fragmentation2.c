/*
 * Copyright 2017-2019, Intel Corporation
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
 * obj_fragmentation.c -- measures average heap external fragmentation
 *
 * This test is based on the workloads proposed in:
 *	Log-structured Memory for DRAM-based Storage
 *	by Stephen M. Rumble, Ankita Kejriwal, and John Ousterhout
 *
 * https://www.usenix.org/system/files/conference/fast14/fast14-paper_rumble.pdf
 */

#include <stdlib.h>
#include "rand.h"
#include "unittest.h"

#define LAYOUT_NAME "obj_fragmentation"

#define MEGABYTE (1UL << 20)
#define GIGABYTE (1UL << 30)

#define RRAND(max, min)\
((min) == (max) ? (min) : (rnd64() % ((max) - (min)) + (min)))

static uint64_t *objects;
static size_t nobjects;
static size_t allocated_current;
#define MAX_OBJECTS (200ULL * 1000000)

#define ALLOC_TOTAL (5000ULL * MEGABYTE)
#define ALLOC_CURR (1000 * MEGABYTE)
#define FREES_P 200

#define DEFAULT_FILE_SIZE (3 * GIGABYTE)

static void
shuffle_objects(size_t start, size_t end)
{
	uint64_t tmp;
	size_t dest;
	for (size_t n = start; n < end; ++n) {
		dest = RRAND(nobjects - 1, 0);
		tmp = objects[n];
		objects[n] = objects[dest];
		objects[dest] = tmp;
	}
}

static uint64_t
remove_last()
{
	UT_ASSERT(nobjects > 0);

	uint64_t obj = objects[--nobjects];

	return obj;
}

static void
allocate_objects(PMEMobjpool *pop, size_t size_min, size_t size_max)
{
	size_t allocated_total = 0;

	size_t sstart = 0;

	PMEMoid oid = pmemobj_root(pop, 1);
	uint64_t uuid_lo = oid.pool_uuid_lo;

	while (allocated_total < ALLOC_TOTAL) {
		size_t s = RRAND(size_max, size_min);
		pmemobj_alloc(pop, &oid, s, 0, NULL, NULL);
		s = pmemobj_alloc_usable_size(oid);

		UT_ASSERTeq(OID_IS_NULL(oid), 0);
		objects[nobjects++] = oid.off;
		UT_ASSERT(nobjects < MAX_OBJECTS);
		allocated_total += s;
		allocated_current += s;

		if (allocated_current > ALLOC_CURR) {
			shuffle_objects(sstart, nobjects);
			for (int i = 0; i < FREES_P; ++i) {
				oid.pool_uuid_lo = uuid_lo;
				oid.off = remove_last();
				allocated_current -=
					pmemobj_alloc_usable_size(oid);
				pmemobj_free(&oid);
			}
			sstart = nobjects;
		}
	}
}

static void
delete_objects(PMEMobjpool *pop, float pct)
{
	size_t nfree = (size_t)(nobjects * pct);

	PMEMoid oid = pmemobj_root(pop, 1);
	uint64_t uuid_lo = oid.pool_uuid_lo;

	shuffle_objects(0, nobjects);
	while (nfree--) {
		oid.off = remove_last();
		oid.pool_uuid_lo = uuid_lo;

		allocated_current -= pmemobj_alloc_usable_size(oid);

		pmemobj_free(&oid);
	}
}

typedef void workload(PMEMobjpool *pop);

static void w0(PMEMobjpool *pop) {
	allocate_objects(pop, 100, 100);
}

static void w1(PMEMobjpool *pop) {
	allocate_objects(pop, 100, 100);
	allocate_objects(pop, 130, 130);
}

static void w2(PMEMobjpool *pop) {
	allocate_objects(pop, 100, 100);
	delete_objects(pop, 0.9F);
	allocate_objects(pop, 130, 130);
}

static void w3(PMEMobjpool *pop) {
	allocate_objects(pop, 100, 150);
	allocate_objects(pop, 200, 250);
}

static void w4(PMEMobjpool *pop) {
	allocate_objects(pop, 100, 150);
	delete_objects(pop, 0.9F);
	allocate_objects(pop, 200, 250);
}

static void w5(PMEMobjpool *pop) {
	allocate_objects(pop, 100, 200);
	delete_objects(pop, 0.5);
	allocate_objects(pop, 1000, 2000);
}

static void w6(PMEMobjpool *pop) {
	allocate_objects(pop, 1000, 2000);
	delete_objects(pop, 0.9F);
	allocate_objects(pop, 1500, 2500);
}

static void w7(PMEMobjpool *pop) {
	allocate_objects(pop, 50, 150);
	delete_objects(pop, 0.9F);
	allocate_objects(pop, 5000, 15000);
}

static void w8(PMEMobjpool *pop) {
	allocate_objects(pop, 2 * MEGABYTE, 2 * MEGABYTE);
}

static workload *workloads[] = {
	w0, w1, w2, w3, w4, w5, w6, w7, w8
};

static float workloads_target[] = {
	0.01f, 0.01f, 0.01f, 0.9f, 0.8f, 0.7f, 0.3f, 0.8f, 0.73f
};

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_fragmentation2");

	if (argc < 3)
		UT_FATAL("usage: %s filename workload [seed]", argv[0]);

	const char *path = argv[1];

	PMEMobjpool *pop = pmemobj_create(path, LAYOUT_NAME, DEFAULT_FILE_SIZE,
				S_IWUSR | S_IRUSR);
	if (pop == NULL)
		UT_FATAL("!pmemobj_create: %s", path);

	int w = atoi(argv[2]);

	if (argc > 3)
		randomize((unsigned)atoi(argv[3]));
	else
		randomize(0);

	objects = ZALLOC(sizeof(uint64_t) * MAX_OBJECTS);
	UT_ASSERTne(objects, NULL);

	workloads[w](pop);

	PMEMoid oid;
	size_t remaining = 0;
	size_t chunk = 100; /* calc at chunk level */
	while (pmemobj_alloc(pop, &oid, chunk, 0, NULL, NULL) == 0)
		remaining += pmemobj_alloc_usable_size(oid) + 16;

	size_t allocated_sum = 0;
	oid = pmemobj_root(pop, 1);
	for (size_t n = 0; n < nobjects; ++n) {
		if (objects[n] == 0)
			continue;
		oid.off = objects[n];
		allocated_sum += pmemobj_alloc_usable_size(oid) + 16;
	}
	size_t used = DEFAULT_FILE_SIZE - remaining;
	float frag = ((float)used / allocated_sum) - 1.f;

	UT_ASSERT(frag <= workloads_target[w]);

	pmemobj_close(pop);

	FREE(objects);

	DONE(NULL);
}

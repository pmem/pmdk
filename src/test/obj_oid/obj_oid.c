/*
 * Copyright 2015-2017, Intel Corporation
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
 * obj_oid.c -- unit test for the reverse direct operation
 */
#include "unittest.h"
#include "lane.h"
#include "obj.h"

#define MAX_PATH_LEN 255
#define LAYOUT_NAME "direct"

pthread_mutex_t lock;
pthread_cond_t cond;
int flag = 1;

PMEMoid thread_oid;

/*
 * test_worker -- (internal) test worker thread
 */
static void *
test_worker(void *arg)
{
	pthread_mutex_lock(&lock);
	/* before pool is closed */
	void *direct = pmemobj_direct(thread_oid);
	UT_ASSERT(OID_EQUALS(thread_oid, pmemobj_oid(direct)));

	flag = 0;
	pthread_cond_signal(&cond);
	pthread_mutex_unlock(&lock);

	pthread_mutex_lock(&lock);
	while (flag == 0)
		pthread_cond_wait(&cond, &lock);
	/* after pool is closed */
	UT_ASSERT(OID_IS_NULL(pmemobj_oid(direct)));

	pthread_mutex_unlock(&lock);

	return NULL;
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_oid");

	if (argc != 3)
		UT_FATAL("usage: %s [directory] [# of pools]", argv[0]);

	pthread_mutex_init(&lock, NULL);
	pthread_cond_init(&cond, NULL);

	int npools = atoi(argv[2]);
	const char *dir = argv[1];
	int r;

	PMEMobjpool **pops = MALLOC(npools * sizeof(PMEMoid *));

	char path[MAX_PATH_LEN];
	for (int i = 0; i < npools; ++i) {
		snprintf(path, MAX_PATH_LEN, "%s/testfile%d", dir, i);
		pops[i] = pmemobj_create(path, LAYOUT_NAME, PMEMOBJ_MIN_POOL,
				S_IWUSR | S_IRUSR);

		if (pops[i] == NULL)
			UT_FATAL("!pmemobj_create");
	}

	PMEMoid *oids = MALLOC(npools * sizeof(PMEMoid));
	PMEMoid *tmpoids = MALLOC(npools * sizeof(PMEMoid));

	UT_ASSERT(OID_IS_NULL(pmemobj_oid(NULL)));
	oids[0] = OID_NULL;

	for (int i = 0; i < npools; ++i) {
		uint64_t off = pops[i]->heap_offset;
		oids[i] = (PMEMoid) {pops[i]->uuid_lo, off};
		UT_ASSERT(OID_EQUALS(oids[i],
			pmemobj_oid(pmemobj_direct(oids[i]))));

		r = pmemobj_alloc(pops[i], &tmpoids[i], 100, 1, NULL, NULL);
		UT_ASSERTeq(r, 0);
		UT_ASSERT(OID_EQUALS(tmpoids[i],
			pmemobj_oid(pmemobj_direct(tmpoids[i]))));
	}

	r = pmemobj_alloc(pops[0], &thread_oid, 100, 2, NULL, NULL);
	UT_ASSERTeq(r, 0);
	UT_ASSERT(!OID_IS_NULL(pmemobj_oid(pmemobj_direct(thread_oid))));

	pthread_mutex_lock(&lock);

	pthread_t t;
	PTHREAD_CREATE(&t, NULL, test_worker, NULL);

	/* wait for the thread to perform the first direct */
	while (flag != 0)
		pthread_cond_wait(&cond, &lock);

	for (int i = 0; i < npools; ++i) {
		pmemobj_free(&tmpoids[i]);

		UT_ASSERT(OID_IS_NULL(pmemobj_oid(
				pmemobj_direct(tmpoids[i]))));
		pmemobj_close(pops[i]);
		UT_ASSERT(OID_IS_NULL(pmemobj_oid(
						pmemobj_direct(oids[i]))));
	}

	/* signal the waiting thread */
	flag = 1;
	pthread_cond_signal(&cond);
	pthread_mutex_unlock(&lock);

	PTHREAD_JOIN(t, NULL);

	FREE(tmpoids);
	FREE(oids);
	FREE(pops);

	pthread_mutex_destroy(&lock);
	pthread_cond_destroy(&cond);

	DONE(NULL);
}

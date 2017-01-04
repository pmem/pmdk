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
 * obj_direct.c -- unit test for direct
 */
#include "obj.h"
#include "unittest.h"

#define MAX_PATH_LEN 255
#define LAYOUT_NAME "direct"

pthread_mutex_t lock1;
pthread_mutex_t lock2;
pthread_cond_t sync_cond1;
pthread_cond_t sync_cond2;
int cond1;
int cond2;
PMEMoid thread_oid;

static void *
test_worker(void *arg)
{
	/* check before pool is closed, then let main continue */
	UT_ASSERTne(pmemobj_direct(thread_oid), NULL);
	pthread_mutex_lock(&lock1);
	cond1 = 1;
	pthread_cond_signal(&sync_cond1);
	pthread_mutex_unlock(&lock1);

	/* wait for main thread to free & close, then check */
	pthread_mutex_lock(&lock2);
	while (!cond2)
		pthread_cond_wait(&sync_cond2, &lock2);
	pthread_mutex_unlock(&lock2);
	UT_ASSERTeq(pmemobj_direct(thread_oid), NULL);
	return NULL;
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_direct");

	if (argc != 3)
		UT_FATAL("usage: %s [directory] [# of pools]", argv[0]);

	int npools = atoi(argv[2]);
	const char *dir = argv[1];
	int r;

	pthread_mutex_init(&lock1, NULL);
	pthread_mutex_init(&lock2, NULL);
	pthread_cond_init(&sync_cond1, NULL);
	pthread_cond_init(&sync_cond2, NULL);
	cond1 = cond2 = 0;

	PMEMobjpool **pops = MALLOC(npools * sizeof(PMEMobjpool *));
	UT_ASSERTne(pops, NULL);

	char path[MAX_PATH_LEN];
	for (int i = 0; i < npools; ++i) {
		snprintf(path, MAX_PATH_LEN, "%s/testfile%d", dir, i);
		pops[i] = pmemobj_create(path, LAYOUT_NAME, PMEMOBJ_MIN_POOL,
				S_IWUSR | S_IRUSR);

		if (pops[i] == NULL)
			UT_FATAL("!pmemobj_create");
	}

	PMEMoid *oids = MALLOC(npools * sizeof(PMEMoid));
	UT_ASSERTne(oids, NULL);
	PMEMoid *tmpoids = MALLOC(npools * sizeof(PMEMoid));
	UT_ASSERTne(tmpoids, NULL);

	oids[0] = OID_NULL;
	UT_ASSERTeq(pmemobj_direct(oids[0]), NULL);

	for (int i = 0; i < npools; ++i) {
		oids[i] = (PMEMoid) {pops[i]->uuid_lo, 0};
		UT_ASSERTeq(pmemobj_direct(oids[i]), NULL);

		uint64_t off = pops[i]->heap_offset;
		oids[i] = (PMEMoid) {pops[i]->uuid_lo, off};
		UT_ASSERTeq((char *)pmemobj_direct(oids[i]) - off,
			(char *)pops[i]);

		r = pmemobj_alloc(pops[i], &tmpoids[i], 100, 1, NULL, NULL);
		UT_ASSERTeq(r, 0);
	}

	r = pmemobj_alloc(pops[0], &thread_oid, 100, 2, NULL, NULL);
	UT_ASSERTeq(r, 0);
	UT_ASSERTne(pmemobj_direct(thread_oid), NULL);

	pthread_t t;
	PTHREAD_CREATE(&t, NULL, test_worker, NULL);

	/* wait for the worker thread to perform the first check */
	pthread_mutex_lock(&lock1);
	while (!cond1)
		pthread_cond_wait(&sync_cond1, &lock1);
	pthread_mutex_unlock(&lock1);

	for (int i = 0; i < npools; ++i) {
		UT_ASSERTne(pmemobj_direct(tmpoids[i]), NULL);

		pmemobj_free(&tmpoids[i]);

		UT_ASSERTeq(pmemobj_direct(tmpoids[i]), NULL);
		pmemobj_close(pops[i]);
		UT_ASSERTeq(pmemobj_direct(oids[i]), NULL);
	}

	/* signal the worker that we're free and closed */
	pthread_mutex_lock(&lock2);
	cond2 = 1;
	pthread_cond_signal(&sync_cond2);
	pthread_mutex_unlock(&lock2);

	PTHREAD_JOIN(t, NULL);
	pthread_cond_destroy(&sync_cond1);
	pthread_cond_destroy(&sync_cond2);
	pthread_mutex_destroy(&lock1);
	pthread_mutex_destroy(&lock2);
	FREE(pops);
	FREE(tmpoids);
	FREE(oids);

	DONE(NULL);
}

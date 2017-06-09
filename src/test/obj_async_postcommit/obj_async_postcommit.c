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
 * obj_async_postcommit.c -- tests for asynchronous postcommit CTL entry points
 *
 * This test runs N threads that populate lane transaction section, M threads
 * that perform asynchronous cleanup of that section, and sets a queue depth
 * to check if the transactions with these settings can be properly performed.
 */

#include "unittest.h"

#define LAYOUT "obj_async_postcommit"

#define OIDS_PER_WORKER 10000
#define OIDS_PER_TX 10

struct worker_args {
	PMEMobjpool *pop;
	PMEMoid *oids;
};

static void *
worker(void *args)
{
	struct worker_args *wa = args;

	int freed = 0;
	do {
		TX_BEGIN(wa->pop) {
			for (int i = 0; i < OIDS_PER_TX; ++i) {
				pmemobj_tx_free(wa->oids[freed++]);
			}
		} TX_ONABORT {
			UT_ASSERT(0);
		} TX_END
	} while (freed != OIDS_PER_WORKER);

	return NULL;
}

static void *
postcommit_worker(void *arg)
{
	PMEMobjpool *pop = (PMEMobjpool *)arg;
	int ret = pmemobj_ctl_get(pop, "tx.post_commit.worker", pop);
	UT_ASSERTeq(ret, 0);

	return NULL;
}

static void
run_test(PMEMobjpool *pop, int nworkers_pc, int nworkers, int qdepth)
{
	os_thread_t *th_pc = MALLOC(sizeof(*th_pc) * nworkers_pc);

	int ret = pmemobj_ctl_set(pop, "tx.post_commit.queue_depth", &qdepth);
	UT_ASSERTeq(ret, 0);
	for (int i = 0; i < nworkers_pc; ++i) {
		PTHREAD_CREATE(&th_pc[i],
			NULL, postcommit_worker, pop);
	}

	os_thread_t *th = MALLOC(sizeof(*th) * nworkers);
	struct worker_args *args = MALLOC(sizeof(*args) * nworkers);
	for (int i = 0; i < nworkers; ++i) {
		args[i].pop = pop;
		args[i].oids = MALLOC(sizeof(PMEMoid) * OIDS_PER_WORKER);
		for (int j = 0; j < OIDS_PER_WORKER; ++j) {
			int ret = pmemobj_alloc(pop,
				&args[i].oids[j], 1, 1, NULL, NULL);
			UT_ASSERTeq(ret, 0);
		}
	}

	for (int i = 0; i < nworkers; ++i) {
		PTHREAD_CREATE(&th[i], NULL, worker, &args[i]);
	}

	for (int i = 0; i < nworkers; ++i) {
		PTHREAD_JOIN(th[i], NULL);
		FREE(args[i].oids);
	}

	ret = pmemobj_ctl_get(pop, "tx.post_commit.stop", pop);
	UT_ASSERTeq(ret, 0);
	for (int i = 0; i < nworkers_pc; ++i)
		PTHREAD_JOIN(th_pc[i], NULL);

	FREE(args);
	FREE(th);
	FREE(th_pc);
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_async_postcommit");

	if (argc != 2)
		UT_FATAL("usage: %s file-name", argv[0]);

	const char *path = argv[1];

	PMEMobjpool *pop;
	if ((pop = pmemobj_create(path, LAYOUT, PMEMOBJ_MIN_POOL * 10,
			S_IWUSR | S_IRUSR)) == NULL)
		UT_FATAL("!pmemobj_create: %s", path);

	run_test(pop, 0, 2, 0);
	run_test(pop, 1, 2, 128);
	run_test(pop, 4, 4, 512);
	run_test(pop, 1, 4, 1024);
	run_test(pop, 0, 2, 0);

	pmemobj_close(pop);

	DONE(NULL);
}

/*
 * Copyright 2015-2020, Intel Corporation
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
 * obj_direct.c -- unit test for pmemobj_direct()
 */
#include "obj.h"
#include "obj_direct.h"
#include "sys_util.h"
#include "unittest.h"

#define MAX_PATH_LEN 255
#define LAYOUT_NAME "direct"

static os_mutex_t lock1;
static os_mutex_t lock2;
static os_cond_t sync_cond1;
static os_cond_t sync_cond2;
static int cond1;
static int cond2;
static PMEMoid thread_oid;

static void *
obj_direct(PMEMoid oid)
{
	void *ptr1 = obj_direct_inline(oid);
	void *ptr2 = obj_direct_non_inline(oid);
	UT_ASSERTeq(ptr1, ptr2);
	return ptr1;
}

static void *
test_worker(void *arg)
{
	/* check before pool is closed, then let main continue */
	UT_ASSERTne(obj_direct(thread_oid), NULL);
	util_mutex_lock(&lock1);
	cond1 = 1;
	os_cond_signal(&sync_cond1);
	util_mutex_unlock(&lock1);

	/* wait for main thread to free & close, then check */
	util_mutex_lock(&lock2);
	while (!cond2)
		os_cond_wait(&sync_cond2, &lock2);
	util_mutex_unlock(&lock2);
	UT_ASSERTeq(obj_direct(thread_oid), NULL);
	return NULL;
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_direct");

	if (argc != 3)
		UT_FATAL("usage: %s [directory] [# of pools]", argv[0]);

	unsigned npools = ATOU(argv[2]);
	const char *dir = argv[1];
	int r;

	util_mutex_init(&lock1);
	util_mutex_init(&lock2);
	util_cond_init(&sync_cond1);
	util_cond_init(&sync_cond2);
	cond1 = cond2 = 0;

	PMEMobjpool **pops = MALLOC(npools * sizeof(PMEMobjpool *));
	UT_ASSERTne(pops, NULL);

	size_t length = strlen(dir) + MAX_PATH_LEN;
	char *path = MALLOC(length);
	for (unsigned i = 0; i < npools; ++i) {
		int ret = snprintf(path, length, "%s"OS_DIR_SEP_STR"testfile%d",
			dir, i);
		if (ret < 0 || ret >= length)
			UT_FATAL("snprintf: %d", ret);
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
	UT_ASSERTeq(obj_direct(oids[0]), NULL);

	for (unsigned i = 0; i < npools; ++i) {
		oids[i] = (PMEMoid) {pops[i]->uuid_lo, 0};
		UT_ASSERTeq(obj_direct(oids[i]), NULL);

		uint64_t off = pops[i]->heap_offset;
		oids[i] = (PMEMoid) {pops[i]->uuid_lo, off};
		UT_ASSERTeq((char *)obj_direct(oids[i]) - off,
			(char *)pops[i]);

		r = pmemobj_alloc(pops[i], &tmpoids[i], 100, 1, NULL, NULL);
		UT_ASSERTeq(r, 0);
	}

	r = pmemobj_alloc(pops[0], &thread_oid, 100, 2, NULL, NULL);
	UT_ASSERTeq(r, 0);
	UT_ASSERTne(obj_direct(thread_oid), NULL);

	os_thread_t t;
	PTHREAD_CREATE(&t, NULL, test_worker, NULL);

	/* wait for the worker thread to perform the first check */
	util_mutex_lock(&lock1);
	while (!cond1)
		os_cond_wait(&sync_cond1, &lock1);
	util_mutex_unlock(&lock1);

	for (unsigned i = 0; i < npools; ++i) {
		UT_ASSERTne(obj_direct(tmpoids[i]), NULL);

		pmemobj_free(&tmpoids[i]);

		UT_ASSERTeq(obj_direct(tmpoids[i]), NULL);
		pmemobj_close(pops[i]);
		UT_ASSERTeq(obj_direct(oids[i]), NULL);
	}

	/* signal the worker that we're free and closed */
	util_mutex_lock(&lock2);
	cond2 = 1;
	os_cond_signal(&sync_cond2);
	util_mutex_unlock(&lock2);

	PTHREAD_JOIN(&t, NULL);
	util_cond_destroy(&sync_cond1);
	util_cond_destroy(&sync_cond2);
	util_mutex_destroy(&lock1);
	util_mutex_destroy(&lock2);
	FREE(pops);
	FREE(tmpoids);
	FREE(oids);

	DONE(NULL);
}

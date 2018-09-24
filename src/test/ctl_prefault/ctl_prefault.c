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
 * ctl_prefault.c -- tests for the ctl entry points: prefault
 */

#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include "unittest.h"

#define OBJ_STR "obj"
#define BLK_STR "blk"
#define LOG_STR "log"

#define BSIZE 20
#define LAYOUT "obj_ctl_prefault"

#ifdef __FreeBSD__
typedef char vec_t;
#else
typedef unsigned char vec_t;
#endif

typedef int (*fun)(void *, const char *, void *);

/*
 * prefault_fun -- function ctl_get/set testing
 */
static void
prefault_fun(int prefault, fun get_func, fun set_func)
{
	int ret;
	int arg;
	int arg_read;

	if (prefault == 1) { /* prefault at open */
		arg_read = -1;
		ret = get_func(NULL, "prefault.at_open", &arg_read);
		UT_ASSERTeq(ret, 0);
		UT_ASSERTeq(arg_read, 0);

		arg = 1;
		ret = set_func(NULL, "prefault.at_open", &arg);
		UT_ASSERTeq(ret, 0);
		UT_ASSERTeq(arg, 1);

		arg_read = -1;
		ret = get_func(NULL, "prefault.at_open", &arg_read);
		UT_ASSERTeq(ret, 0);
		UT_ASSERTeq(arg_read, 1);

	} else if (prefault == 2) { /* prefault at create */
		arg_read = -1;
		ret = get_func(NULL, "prefault.at_create", &arg_read);
		UT_ASSERTeq(ret, 0);
		UT_ASSERTeq(arg_read, 0);

		arg = 1;
		ret = set_func(NULL, "prefault.at_create", &arg);
		UT_ASSERTeq(ret, 0);
		UT_ASSERTeq(arg, 1);

		arg_read = -1;
		ret = get_func(NULL, "prefault.at_create", &arg_read);
		UT_ASSERTeq(ret, 0);
		UT_ASSERTeq(arg_read, 1);
	}
}
/*
 * count_resident_pages -- count resident_pages
 */
static size_t
count_resident_pages(void *pool, size_t length)
{
	size_t arr_len = (length + Ut_pagesize - 1) / Ut_pagesize;
	vec_t *vec = MALLOC(sizeof(*vec) * arr_len);

	int ret = mincore(pool, length, vec);
	UT_ASSERTeq(ret, 0);

	size_t resident_pages = 0;
	for (size_t i = 0; i < arr_len; ++i)
		resident_pages += vec[i] & 0x1;

	FREE(vec);

	return resident_pages;
}
/*
 * test_obj -- open/create PMEMobjpool
 */
static void
test_obj(const char *path, int open)
{
	PMEMobjpool *pop;
	if (open) {
		if ((pop = pmemobj_open(path, LAYOUT)) == NULL)
			UT_FATAL("!pmemobj_open: %s", path);
	} else {
		if ((pop = pmemobj_create(path, LAYOUT,
				PMEMOBJ_MIN_POOL,
				S_IWUSR | S_IRUSR)) == NULL)
			UT_FATAL("!pmemobj_create: %s", path);
	}

	size_t resident_pages = count_resident_pages(pop, PMEMOBJ_MIN_POOL);

	pmemobj_close(pop);

	UT_OUT("%ld", resident_pages);
}
/*
 * test_blk -- open/create PMEMblkpool
 */
static void
test_blk(const char *path, int open)
{
	PMEMblkpool *pbp;
	if (open) {
		if ((pbp = pmemblk_open(path, BSIZE)) == NULL)
			UT_FATAL("!pmemblk_open: %s", path);
	} else {
		if ((pbp = pmemblk_create(path, BSIZE, PMEMBLK_MIN_POOL,
			S_IWUSR | S_IRUSR)) == NULL)
			UT_FATAL("!pmemblk_create: %s", path);
	}

	size_t resident_pages = count_resident_pages(pbp, PMEMBLK_MIN_POOL);

	pmemblk_close(pbp);

	UT_OUT("%ld", resident_pages);
}
/*
 * test_log -- open/create PMEMlogpool
 */
static void
test_log(const char *path, int open)
{
	PMEMlogpool *plp;

	/*
	 * To test prefaulting, pool must have size at least equal to 2 pages.
	 * If 2MB huge pages are used this is at least 4MB.
	 */
	size_t pool_size = 2 * PMEMLOG_MIN_POOL;

	if (open) {
		if ((plp = pmemlog_open(path)) == NULL)
			UT_FATAL("!pmemlog_open: %s", path);
	} else {
		if ((plp = pmemlog_create(path, pool_size,
				S_IWUSR | S_IRUSR)) == NULL)
			UT_FATAL("!pmemlog_create: %s", path);
	}

	size_t resident_pages = count_resident_pages(plp, pool_size);

	pmemlog_close(plp);

	UT_OUT("%ld", resident_pages);
}

#define USAGE() do {\
	UT_FATAL("usage: %s file-name type(obj/blk/log) prefault(0/1/2) "\
			"open(0/1)", argv[0]);\
} while (0)

int
main(int argc, char *argv[])
{
	START(argc, argv, "ctl_prefault");

	if (argc != 5)
		USAGE();

	char *type = argv[1];
	const char *path = argv[2];
	int prefault = atoi(argv[3]);
	int open = atoi(argv[4]);

	if (strcmp(type, OBJ_STR) == 0) {
		prefault_fun(prefault, (fun)pmemobj_ctl_get,
				(fun)pmemobj_ctl_set);
		test_obj(path, open);
	} else if (strcmp(type, BLK_STR) == 0) {
		prefault_fun(prefault, (fun)pmemblk_ctl_get,
				(fun)pmemblk_ctl_set);
		test_blk(path, open);
	} else if (strcmp(type, LOG_STR) == 0) {
		prefault_fun(prefault, (fun)pmemlog_ctl_get,
				(fun)pmemlog_ctl_set);
		test_log(path, open);
	} else
		USAGE();

	DONE(NULL);
}

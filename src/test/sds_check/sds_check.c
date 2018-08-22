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
 * sds_check.c -- unit test for disabled shutdown_state_check
 */

#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include "unittest.h"
#include "shutdown_state.h"

#define OBJ_STR "obj"
#define BLK_STR "blk"
#define LOG_STR "log"

#define BSIZE 20
#define LAYOUT "obj_sds"

#ifdef __FreeBSD__
typedef char vec_t;
#else
typedef unsigned char vec_t;
#endif

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

	pmemobj_close(pop);
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

	pmemblk_close(pbp);
}
/*
 * test_log -- open/create PMEMlogpool
 */
static void
test_log(const char *path, int open)
{
	PMEMlogpool *plp;
	if (open) {
		if ((plp = pmemlog_open(path)) == NULL)
			UT_FATAL("!pmemlog_open: %s", path);
	} else {
		if ((plp = pmemlog_create(path, PMEMLOG_MIN_POOL,
				S_IWUSR | S_IRUSR)) == NULL)
			UT_FATAL("!pmemlog_create: %s", path);
	}

	pmemlog_close(plp);
}

#define USAGE() do {\
	UT_FATAL("usage: %s file-name type(obj/blk/log)"\
			"open(0/1)", argv[0]);\
} while (0)

int
main(int argc, char *argv[])
{
	START(argc, argv, "sds_check");

	if (argc != 4)
		USAGE();

	char *type = argv[1];
	const char *path = argv[2];
	int open = atoi(argv[3]);

	if (strcmp(type, OBJ_STR) == 0) {
		test_obj(path, open);
	} else if (strcmp(type, BLK_STR) == 0) {
		test_blk(path, open);
	} else if (strcmp(type, LOG_STR) == 0) {
		test_log(path, open);
	} else
		USAGE();

	DONE(NULL);
}

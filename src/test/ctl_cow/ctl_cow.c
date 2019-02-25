/*
 * Copyright 2019, Intel Corporation
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
 * ctl_cow.c -- unit tests for copy on write feature which check
 * if changes are reverted after pool close when copy_on_write.at_open = 1
 */

#include <stddef.h>
#include "unittest.h"
#include <string.h>

struct test_st {
	int x;
};

POBJ_LAYOUT_BEGIN(test_layout);
	POBJ_LAYOUT_ROOT(test_layout, struct my_root);
	POBJ_LAYOUT_TOID(test_layout, struct test_st);
POBJ_LAYOUT_END(test_layout);

struct my_root {
	TOID(struct test_st) x;
	TOID(struct test_st) y;
	TOID(struct test_st) z;
};

static void
test_obj(const char *path)
{
	PMEMobjpool *pop = pmemobj_open(path, NULL);
	if (pop == NULL)
		UT_FATAL("!%s: pmemobj_open", path);

	TOID(struct my_root) root = POBJ_ROOT(pop, struct my_root);

	TX_BEGIN(pop) {
		TX_ADD(root);
		TOID(struct test_st) x = TX_NEW(struct test_st);
		TOID(struct test_st) y = TX_NEW(struct test_st);
		TOID(struct test_st) z = TX_NEW(struct test_st);
		D_RW(x)->x = 5;
		D_RW(y)->x = 10;
		D_RW(z)->x = 15;
		D_RW(root)->x = x;
		D_RW(root)->y = y;
		D_RW(root)->z = z;
	} TX_ONABORT {
		abort();
	} TX_END

	TX_BEGIN(pop) {
		TX_ADD(root);
		TX_FREE(D_RW(root)->x);
		D_RW(root)->x = TOID_NULL(struct test_st);

		TX_ADD(D_RW(root)->y);
		TOID(struct test_st) y = D_RO(root)->y;
		D_RW(y)->x = 100;
	} TX_ONABORT {
		abort();
	} TX_END

	pmemobj_close(pop);
}

static void
test_blk(const char *path)
{
	PMEMblkpool *pbp = pmemblk_open(path, 512);

	if (pbp == NULL)
		UT_FATAL("!cannot open %s", path);

	char x[512] = "Test blk x";
	char y[512] = "Test blk y";

	if (pmemblk_write(pbp, &x, 1) < 0)
		UT_FATAL("cannot write to %s", path);

	if (pmemblk_write(pbp, &y, 2) < 0)
		UT_FATAL("cannot write to %s", path);

	if (pmemblk_set_zero(pbp, 2) < 0)
		UT_FATAL("cannot write to %s", path);

	pmemblk_close(pbp);
}

static void
test_log(const char *path)
{
	PMEMlogpool *plp = pmemlog_open(path);

	if (plp == NULL)
		UT_FATAL("!cannot open %s", path);

	char buf[] = "pmemlog test";
	char buf_2[] = "pmemlog test 2";

	if (pmemlog_append(plp, buf, strlen(buf)) < 0)
		UT_FATAL("cannot append to %s", path);

	if (pmemlog_append(plp, buf_2, strlen(buf_2)) < 0)
		UT_FATAL("cannot append to %s", path);

	pmemlog_close(plp);
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "ctl_cow");

	if (argc < 3)
		UT_FATAL("usage: %s filename obj|log|blk", argv[0]);

	const char *path = argv[1];
	const char *action = argv[2];

	if (strcmp(action, "obj") == 0) {
		test_obj(path);

	} else if (strcmp(action, "blk") == 0) {
		test_blk(path);

	} else if (strcmp(action, "log") == 0) {
		test_log(path);

	} else {
		UT_FATAL("%s is not a valid action", action);
	}

	DONE(NULL);
}

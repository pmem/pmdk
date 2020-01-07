// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2018, Intel Corporation */

/*
 * util_is_zeroed.c -- unit test for util_is_zeroed
 */

#include "unittest.h"
#include "util.h"

int
main(int argc, char *argv[])
{
	START(argc, argv, "util_is_zeroed");

	util_init();

	char bigbuf[3000];
	memset(bigbuf + 0,    0x11, 1000);
	memset(bigbuf + 1000, 0x0,  1000);
	memset(bigbuf + 2000, 0xff, 1000);

	UT_ASSERTeq(util_is_zeroed(bigbuf, 1000), 0);
	UT_ASSERTeq(util_is_zeroed(bigbuf + 1000, 1000), 1);
	UT_ASSERTeq(util_is_zeroed(bigbuf + 2000, 1000), 0);

	UT_ASSERTeq(util_is_zeroed(bigbuf, 0), 1);

	UT_ASSERTeq(util_is_zeroed(bigbuf + 999, 1000), 0);
	UT_ASSERTeq(util_is_zeroed(bigbuf + 1000, 1001), 0);
	UT_ASSERTeq(util_is_zeroed(bigbuf + 1001, 1000), 0);

	char *buf = bigbuf + 1000;
	buf[0] = 1;
	UT_ASSERTeq(util_is_zeroed(buf, 1000), 0);

	memset(buf, 0, 1000);
	buf[1] = 1;
	UT_ASSERTeq(util_is_zeroed(buf, 1000), 0);

	memset(buf, 0, 1000);
	buf[239] = 1;
	UT_ASSERTeq(util_is_zeroed(buf, 1000), 0);

	memset(buf, 0, 1000);
	buf[999] = 1;
	UT_ASSERTeq(util_is_zeroed(buf, 1000), 0);

	memset(buf, 0, 1000);
	buf[1000] = 1;
	UT_ASSERTeq(util_is_zeroed(buf, 1000), 1);

	DONE(NULL);
}

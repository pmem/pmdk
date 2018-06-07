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

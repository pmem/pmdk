// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2017, Intel Corporation */
/*
 * Copyright (c) 2016, Microsoft Corporation. All rights reserved.
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
 * win_common.c -- test common POSIX or Linux API that were implemented
 * for Windows by our library.
 */

#include "unittest.h"

/*
 * test_setunsetenv - test the setenv and unsetenv APIs
 */
static void
test_setunsetenv(void)
{
	os_unsetenv("TEST_SETUNSETENV_ONE");

	/* set a new variable without overwriting - expect the new value */
	UT_ASSERT(os_setenv("TEST_SETUNSETENV_ONE",
		"test_setunsetenv_one", 0) == 0);
	UT_ASSERT(strcmp(os_getenv("TEST_SETUNSETENV_ONE"),
		"test_setunsetenv_one") == 0);

	/* set an existing variable without overwriting - expect old value */
	UT_ASSERT(os_setenv("TEST_SETUNSETENV_ONE",
		"test_setunsetenv_two", 0) == 0);
	UT_ASSERT(strcmp(os_getenv("TEST_SETUNSETENV_ONE"),
		"test_setunsetenv_one") == 0);

	/* set an existing variable with overwriting - expect the new value */
	UT_ASSERT(os_setenv("TEST_SETUNSETENV_ONE",
		"test_setunsetenv_two", 1) == 0);
	UT_ASSERT(strcmp(os_getenv("TEST_SETUNSETENV_ONE"),
		"test_setunsetenv_two") == 0);

	/* unset our test value - expect it to be empty */
	UT_ASSERT(os_unsetenv("TEST_SETUNSETENV_ONE") == 0);
	UT_ASSERT(os_getenv("TEST_SETUNSETENV_ONE") == NULL);
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "win_common - testing %s",
		(argc > 1) ? argv[1] : "setunsetenv");

	if (argc == 1 || (stricmp(argv[1], "setunsetenv") == 0))
		test_setunsetenv();

	DONE(NULL);
}

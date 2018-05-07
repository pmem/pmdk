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
 * pmem_has_auto_flush_win.c -- unit test for pmem_has_auto_flush_win()
 *
 * usage: pmem_has_auto_flush_win <option>
 * options:
 *     n - is nfit available or not (y or n)
 * type: number of platform capabilities structure
 * capabilities: platform capabilities bits
 */

#include <stdbool.h>
#include <errno.h>
#include "unittest.h"
#include "pmemcommon.h"
#include "set.h"
#include "mocks_windows.h"
#include "pmem_has_auto_flush_win.h"
#include "util.h"

#define LOG_PREFIX "ut"
#define LOG_LEVEL_VAR "TEST_LOG_LEVEL"
#define LOG_FILE_VAR "TEST_LOG_FILE"
#define MAJOR_VERSION 1
#define MINOR_VERSION 0

size_t Is_nfit = 0;
size_t Pc_type = 0;
size_t Pc_capabilities = 3;

int
main(int argc, char *argv[])
{
	START(argc, argv, "pmem_has_auto_flush_win");
	common_init(LOG_PREFIX, LOG_LEVEL_VAR, LOG_FILE_VAR,
			MAJOR_VERSION, MINOR_VERSION);

	if (argc < 4)
		UT_FATAL("usage: pmem_has_auto_flush_win "
				"<option> <type> <capabilities>",
			argv[0]);

	Pc_type = (size_t)atoi(argv[2]);
	Pc_capabilities = (size_t)atoi(argv[3]);
	Is_nfit = argv[1][0] == 'y';

	int eADR = pmem_has_auto_flush();
	UT_OUT("pmem_has_auto_flush ret: %d", eADR);

	common_fini();
	DONE(NULL);
}

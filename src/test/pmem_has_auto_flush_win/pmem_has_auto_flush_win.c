// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2018-2019, Intel Corporation */

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
#include "pmem.h"
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

	pmem_init();

	Pc_type = (size_t)atoi(argv[2]);
	Pc_capabilities = (size_t)atoi(argv[3]);
	Is_nfit = argv[1][0] == 'y';

	int eADR = pmem_has_auto_flush();
	UT_OUT("pmem_has_auto_flush ret: %d", eADR);

	common_fini();
	DONE(NULL);
}

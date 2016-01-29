/*
 * Copyright 2016, Intel Corporation
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
 * libpmempool_test -- test of libpmempool.
 *
 */

#include <stddef.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#include "unittest.h"

/*
 * check_pool -- check given pool
 */
static void
check_pool(const char *path, const char *backup_path, enum pmempool_pool_type
	pool_type, int flags)
{
	const char *status2str[] = {
		[PMEMPOOL_CHECK_RESULT_CONSISTENT]	= "consistent",
		[PMEMPOOL_CHECK_RESULT_NOT_CONSISTENT] = "not consistent",
		[PMEMPOOL_CHECK_RESULT_REPAIRED]	= "repaired",
		[PMEMPOOL_CHECK_RESULT_CANNOT_REPAIR]	= "cannot repair",
		[PMEMPOOL_CHECK_RESULT_ERROR]	= "fatal",
	};

	PMEMpoolcheck *ppc = pmempool_check_init(path, backup_path, pool_type,
		flags);
	if (!ppc) {
		UT_OUT("Error: %s", strerror(errno));
		return;
	}

	struct pmempool_check_status *status = NULL;
	while ((status = pmempool_check(ppc)) != NULL) {
		switch (status->type) {
		case PMEMPOOL_CHECK_MSG_TYPE_ERROR:
			UT_OUT("%s", status->str.msg);
			break;
		case PMEMPOOL_CHECK_MSG_TYPE_INFO:
			UT_OUT("%s", status->str.msg);
			break;
		case PMEMPOOL_CHECK_MSG_TYPE_QUESTION:
			UT_OUT("%s", status->str.msg);
			status->str.answer = "yes";
			break;
		default:
			pmempool_check_end(ppc);
			exit(EXIT_FAILURE);
		}
	}

	enum pmempool_check_result ret = pmempool_check_end(ppc);
	UT_OUT("status = %s", status2str[ret]);
}

/*
 * print_usage -- print usage of program
 */
static void
print_usage(char *name)
{
	UT_OUT("Usage: %s [-t <pool_type>] [-r <repair>] [-d <dry_run>] "
			"[-y <always_yes>] [-f <flags>] [-a <advanced>] "
			"[-b <backup_path>] <pool_path>", name);
}

/*
 * set_flag -- parse the value and set the flag according to a obtained value
 */
static void
set_flag(const char *value, int *flags, int flag)
{
	if (atoi(value) > 0)
		*flags |= flag;
	else
		*flags &= ~flag;
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "libpmempool_test");
	int opt;

	const char *path = NULL;
	const char *backup_path = NULL;
	enum pmempool_pool_type pool_type = PMEMPOOL_POOL_TYPE_LOG;
	int flags = PMEMPOOL_CHECK_FORMAT_STR | PMEMPOOL_CHECK_REPAIR |
		PMEMPOOL_CHECK_VERBOSE;

	while ((opt = getopt(argc, argv, "t:r:d:a:y:s:b:n")) != -1) {
		switch (opt) {
		case 't':
			if (strcmp(optarg, "blk") == 0) {
				pool_type = PMEMPOOL_POOL_TYPE_BLK;
			} else if (strcmp(optarg, "log") == 0) {
				pool_type = PMEMPOOL_POOL_TYPE_LOG;
			} else if (strcmp(optarg, "obj") == 0) {
				pool_type = PMEMPOOL_POOL_TYPE_OBJ;
			} else if (strcmp(optarg, "btt") == 0) {
				pool_type = PMEMPOOL_POOL_TYPE_BTT;
			} else {
				pool_type = (uint32_t)strtoul(optarg, NULL, 0);
			}
			break;
		case 'r':
			set_flag(optarg, &flags, PMEMPOOL_CHECK_REPAIR);
			break;
		case 'd':
			set_flag(optarg, &flags, PMEMPOOL_CHECK_DRY_RUN);
			break;
		case 'a':
			set_flag(optarg, &flags, PMEMPOOL_CHECK_ADVANCED);
			break;
		case 'y':
			set_flag(optarg, &flags, PMEMPOOL_CHECK_ALWAYS_YES);
			break;
		case 's':
			set_flag(optarg, &flags, PMEMPOOL_CHECK_FORMAT_STR);
			break;
		case 'b':
			backup_path = optarg;
			break;
		default:
			print_usage(argv[0]);
			UT_FATAL("unknown option: %c", opt);
		}
	}

	if (optind < argc) {
		path = argv[optind];
	}

	check_pool(path, backup_path, pool_type, flags);

	DONE(NULL);
}

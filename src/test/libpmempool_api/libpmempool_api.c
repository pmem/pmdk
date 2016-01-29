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
 * libpmempool_api -- Placeholder for testing libpmempool API.
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
check_pool(struct pmempool_check_args *args)
{
	const char *status2str[] = {
		[PMEMPOOL_CHECK_RESULT_CONSISTENT]	= "consistent",
		[PMEMPOOL_CHECK_RESULT_NOT_CONSISTENT] = "not consistent",
		[PMEMPOOL_CHECK_RESULT_REPAIRED]	= "repaired",
		[PMEMPOOL_CHECK_RESULT_CANNOT_REPAIR]	= "cannot repair",
		[PMEMPOOL_CHECK_RESULT_ERROR]	= "fatal",
	};

	PMEMpoolcheck *ppc = pmempool_check_init(args);
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
			"[-b <backup_path>] [-n] <pool_path>", name);
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "libpmempool_api");
	int opt;
	int is_null_struct = 0;

	struct pmempool_check_args args = {
		.path		= NULL,
		.pool_type	= PMEMPOOL_POOL_TYPE_LOG,
		.repair		= 1,
		.dry_run	= 0,
		.advanced	= 0,
		.always_yes	= 0,
		.flags		= PMEMPOOL_CHECK_FORMAT_STR,
		.verbose	= 1,
		.backup_path	= NULL
	};

	while ((opt = getopt(argc, argv, "t:r:d:a:y:f:b:n")) != -1) {
		switch (opt) {
		case 't':
			if (strcmp(optarg, "blk") == 0) {
				args.pool_type	= PMEMPOOL_POOL_TYPE_BLK;
			} else if (strcmp(optarg, "log") == 0) {
				args.pool_type	= PMEMPOOL_POOL_TYPE_LOG;
			} else if (strcmp(optarg, "obj") == 0) {
				args.pool_type	= PMEMPOOL_POOL_TYPE_OBJ;
			} else if (strcmp(optarg, "btt") == 0) {
				args.pool_type	= PMEMPOOL_POOL_TYPE_BTT_DEV;
			} else {
				args.pool_type = (uint32_t)strtoul
					(optarg, NULL, 0);
			}
			break;
		case 'r':
			args.repair = atoi(optarg);
			break;
		case 'd':
			args.dry_run = atoi(optarg);
			break;
		case 'a':
			args.advanced = atoi(optarg);
			break;
		case 'y':
			args.always_yes = atoi(optarg);
			break;
		case 'f':
			args.flags = (uint32_t)strtoul(optarg, NULL, 0);
			break;
		case 'b':
			args.backup_path = optarg;
			break;
		case 'n':
			is_null_struct = 1;
			break;
		default:
			print_usage(argv[0]);
			return -1;
		}
	}

	if (optind < argc) {
		args.path = argv[optind];
	}

	if (is_null_struct) {
		check_pool(NULL);
	} else {
		check_pool(&args);
	}

	DONE(NULL);
}

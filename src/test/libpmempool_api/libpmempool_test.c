// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016-2023, Intel Corporation */

/*
 * libpmempool_test -- test of libpmempool.
 *
 */

#include <stddef.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>

#include "unittest.h"

/*
 * Exact copy of the struct pmempool_check_args from libpmempool 1.0 provided to
 * test libpmempool against various pmempool_check_args structure versions.
 */
struct pmempool_check_args_1_0 {
	const char *path;
	const char *backup_path;
	enum pmempool_pool_type pool_type;
	int flags;
};

/*
 * check_pool -- check given pool
 */
static void
check_pool(struct pmempool_check_args *args, size_t args_size)
{
	const char *status2str[] = {
		[PMEMPOOL_CHECK_RESULT_CONSISTENT]	= "consistent",
		[PMEMPOOL_CHECK_RESULT_NOT_CONSISTENT] = "not consistent",
		[PMEMPOOL_CHECK_RESULT_REPAIRED]	= "repaired",
		[PMEMPOOL_CHECK_RESULT_CANNOT_REPAIR]	= "cannot repair",
		[PMEMPOOL_CHECK_RESULT_ERROR]	= "fatal",
	};

	PMEMpoolcheck *ppc = pmempool_check_init(args, args_size);
	if (!ppc) {
		char buff[UT_MAX_ERR_MSG];
		strerror_r(errno, buff, UT_MAX_ERR_MSG);
		UT_OUT("Error: %s", buff);
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

	struct pmempool_check_args_1_0 args = {
		.path		= NULL,
		.backup_path	= NULL,
		.pool_type	= PMEMPOOL_POOL_TYPE_OBJ,
		.flags		= PMEMPOOL_CHECK_FORMAT_STR |
			PMEMPOOL_CHECK_REPAIR | PMEMPOOL_CHECK_VERBOSE
	};

	size_t args_size = sizeof(struct pmempool_check_args_1_0);

	while ((opt = getopt(argc, argv, "t:r:d:a:y:s:b:")) != -1) {
		switch (opt) {
		case 't':
			if (strcmp(optarg, "obj") == 0) {
				args.pool_type = PMEMPOOL_POOL_TYPE_OBJ;
			} else {
				args.pool_type =
					(uint32_t)strtoul(optarg, NULL, 0);
			}
			break;
		case 'r':
			set_flag(optarg, &args.flags, PMEMPOOL_CHECK_REPAIR);
			break;
		case 'd':
			set_flag(optarg, &args.flags, PMEMPOOL_CHECK_DRY_RUN);
			break;
		case 'a':
			set_flag(optarg, &args.flags, PMEMPOOL_CHECK_ADVANCED);
			break;
		case 'y':
			set_flag(optarg, &args.flags,
				PMEMPOOL_CHECK_ALWAYS_YES);
			break;
		case 's':
			args_size = strtoul(optarg, NULL, 0);
			break;
		case 'b':
			args.backup_path = optarg;
			break;
		default:
			print_usage(argv[0]);
			UT_FATAL("unknown option: %c", opt);
		}
	}

	if (optind < argc) {
		args.path = argv[optind];
	}

	check_pool((struct pmempool_check_args *)&args, args_size);

	DONE(NULL);
}

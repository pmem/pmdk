// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2017, Intel Corporation */

/*
 * libpmempool_test_win -- test of libpmempool.
 *
 */

#include <stddef.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#include "unittest.h"

/*
 * Exact copy of the struct pmempool_check_args from libpmempool 1.0 provided to
 * test libpmempool against various pmempool_check_args structure versions.
 */
struct pmempool_check_args_1_0 {
	const wchar_t *path;
	const wchar_t *backup_path;
	enum pmempool_pool_type pool_type;
	int flags;
};

/*
 * check_pool -- check given pool
 */
static void
check_pool(struct pmempool_check_argsW *args, size_t args_size)
{
	const char *status2str[] = {
		[PMEMPOOL_CHECK_RESULT_CONSISTENT] = "consistent",
		[PMEMPOOL_CHECK_RESULT_NOT_CONSISTENT] = "not consistent",
		[PMEMPOOL_CHECK_RESULT_REPAIRED] = "repaired",
		[PMEMPOOL_CHECK_RESULT_CANNOT_REPAIR] = "cannot repair",
		[PMEMPOOL_CHECK_RESULT_ERROR] = "fatal",
	};

	PMEMpoolcheck *ppc = pmempool_check_initW(args, args_size);
	if (!ppc) {
		char buff[UT_MAX_ERR_MSG];
		ut_strerror(errno, buff, UT_MAX_ERR_MSG);
		UT_OUT("Error: %s", buff);
		return;
	}

	struct pmempool_check_statusW *status = NULL;
	while ((status = pmempool_checkW(ppc)) != NULL) {
		char *msg = ut_toUTF8(status->str.msg);
		switch (status->type) {
		case PMEMPOOL_CHECK_MSG_TYPE_ERROR:
			UT_OUT("%s", msg);
			break;
		case PMEMPOOL_CHECK_MSG_TYPE_INFO:
			UT_OUT("%s", msg);
			break;
		case PMEMPOOL_CHECK_MSG_TYPE_QUESTION:
			UT_OUT("%s", msg);
			status->str.answer = L"yes";
			break;
		default:
			pmempool_check_end(ppc);
			free(msg);
			exit(EXIT_FAILURE);
		}
		free(msg);
	}

	enum pmempool_check_result ret = pmempool_check_end(ppc);
	UT_OUT("status = %s", status2str[ret]);
}

/*
 * print_usage -- print usage of program
 */
static void
print_usage(wchar_t *name)
{
	UT_OUT("Usage: %S [-t <pool_type>] [-r <repair>] [-d <dry_run>] "
		"[-y <always_yes>] [-f <flags>] [-a <advanced>] "
		"[-b <backup_path>] <pool_path>", name);
}

/*
 * set_flag -- parse the value and set the flag according to a obtained value
 */
static void
set_flag(const wchar_t *value, int *flags, int flag)
{
	if (_wtoi(value) > 0)
		*flags |= flag;
	else
		*flags &= ~flag;
}

int
wmain(int argc, wchar_t *argv[])
{
	STARTW(argc, argv, "libpmempool_test_win");
	struct pmempool_check_args_1_0 args = {
		.path = NULL,
		.backup_path = NULL,
		.pool_type = PMEMPOOL_POOL_TYPE_LOG,
		.flags = PMEMPOOL_CHECK_FORMAT_STR |
		PMEMPOOL_CHECK_REPAIR | PMEMPOOL_CHECK_VERBOSE
	};

	size_t args_size = sizeof(struct pmempool_check_args_1_0);

	for (int i = 1; i < argc - 1; i += 2) {
		wchar_t *optarg = argv[i + 1];
		if (wcscmp(L"-t", argv[i]) == 0) {
			if (wcscmp(optarg, L"blk") == 0) {
				args.pool_type = PMEMPOOL_POOL_TYPE_BLK;
			} else if (wcscmp(optarg, L"log") == 0) {
				args.pool_type = PMEMPOOL_POOL_TYPE_LOG;
			} else if (wcscmp(optarg, L"obj") == 0) {
				args.pool_type = PMEMPOOL_POOL_TYPE_OBJ;
			} else if (wcscmp(optarg, L"btt") == 0) {
				args.pool_type = PMEMPOOL_POOL_TYPE_BTT;
			} else {
				args.pool_type =
					(uint32_t)wcstoul(optarg, NULL, 0);
			}
		} else if (wcscmp(L"-r", argv[i]) == 0) {
			set_flag(optarg, &args.flags, PMEMPOOL_CHECK_REPAIR);
		} else if (wcscmp(L"-d", argv[i]) == 0) {
			set_flag(optarg, &args.flags, PMEMPOOL_CHECK_DRY_RUN);
		} else if (wcscmp(L"-a", argv[i]) == 0) {
			set_flag(optarg, &args.flags, PMEMPOOL_CHECK_ADVANCED);
		} else if (wcscmp(L"-y", argv[i]) == 0) {
			set_flag(optarg, &args.flags,
				PMEMPOOL_CHECK_ALWAYS_YES);
		} else if (wcscmp(L"-s", argv[i]) == 0) {
			args_size = wcstoul(optarg, NULL, 0);
		} else if (wcscmp(L"-b", argv[i]) == 0) {
			args.backup_path = optarg;
		} else {
			print_usage(argv[0]);
			UT_FATAL("unknown option: %c", argv[i][1]);
		}
	}

	args.path = argv[argc - 1];

	check_pool((struct pmempool_check_argsW *)&args, args_size);

	DONEW(NULL);
}

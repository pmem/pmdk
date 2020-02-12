// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2020, Intel Corporation */

/*
 * obj_pool_sets_parser.c -- unit test for parsing a set file
 *
 * usage: obj_pool_sets_parser set-file ...
 */

#include "set.h"
#include "unittest.h"
#include "pmemcommon.h"
#include "fault_injection.h"

#define LOG_PREFIX "parser"
#define LOG_LEVEL_VAR "PARSER_LOG_LEVEL"
#define LOG_FILE_VAR "PARSER_LOG_FILE"
#define MAJOR_VERSION 1
#define MINOR_VERSION 0

int
main(int argc, char *argv[])
{
	START(argc, argv, "util_poolset_parse");

	common_init(LOG_PREFIX, LOG_LEVEL_VAR, LOG_FILE_VAR,
			MAJOR_VERSION, MINOR_VERSION);

	if (argc < 3)
		UT_FATAL("usage: %s set-file-name ...", argv[0]);

	struct pool_set *set;
	int fd;

	switch (argv[1][0]) {
	case 't':
		for (int i = 2; i < argc; i++) {
			const char *path = argv[i];

			fd = OPEN(path, O_RDWR);

			int ret = util_poolset_parse(&set, path, fd);
			if (ret == 0)
				util_poolset_free(set);

			CLOSE(fd);
		}
		break;
	case 'f':
		if (!core_fault_injection_enabled())
			break;

		const char *path = argv[2];
		fd = OPEN(path, O_RDWR);

		core_inject_fault_at(PMEM_MALLOC, 1,
				"util_poolset_directories_load");
		int ret = util_poolset_parse(&set, path, fd);
		UT_ASSERTne(ret, 0);
		UT_ASSERTeq(errno, ENOMEM);

		CLOSE(fd);
	}

	common_fini();

	DONE(NULL);
}

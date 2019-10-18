/*
 * Copyright 2015-2019, Intel Corporation
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
		if (!common_fault_injection_enabled())
			break;

		const char *path = argv[2];
		fd = OPEN(path, O_RDWR);

		common_inject_fault_at(PMEM_MALLOC, 1,
				"util_poolset_directories_load");
		int ret = util_poolset_parse(&set, path, fd);
		UT_ASSERTne(ret, 0);
		UT_ASSERTeq(errno, ENOMEM);

		CLOSE(fd);
	}

	common_fini();

	DONE(NULL);
}

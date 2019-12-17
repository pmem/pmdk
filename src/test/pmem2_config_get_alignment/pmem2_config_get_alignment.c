/*
 * Copyright 2019, Intel Corporation
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
 * pmem2_config_get_alignment.c -- pmem2_config_get_alignment unittests
 */

#include <fcntl.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "unittest.h"
#include "ut_pmem2_utils.h"
#include "ut_pmem2_config.h"
#include "config.h"

typedef void (*test_fun)(const char *path, size_t ref_alignment);

/*
 * test_notset_fd - tests what happens when file descriptor was not set
 */
static void
test_notset_fd(const char *ignored_path, size_t unused)
{
	struct pmem2_config cfg;
	pmem2_config_init(&cfg);
	size_t alignment;
	int ret = pmem2_config_get_alignment(&cfg, &alignment);

	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_FILE_HANDLE_NOT_SET);
}

static void
init_cfg(struct pmem2_config *cfg, int fd)
{
	pmem2_config_init(cfg);
#ifdef _WIN32
	cfg->handle = (HANDLE)_get_osfhandle(fd);
#else
	cfg->fd = fd;
#endif
}

/*
 * test_get_alignment_success - simply checks returned value
 */
static void
test_get_alignment_success(const char *path, size_t ref_alignment)
{
	int fd = OPEN(path, O_RDWR);

	struct pmem2_config cfg;
	init_cfg(&cfg, fd);

	size_t alignment;
	int ret = pmem2_config_get_alignment(&cfg, &alignment);

	UT_PMEM2_EXPECT_RETURN(ret, 0);
	UT_ASSERTeq(ref_alignment, alignment);

	CLOSE(fd);
}

/*
 * test_directory - tests directory path
 */
static void
test_directory(const char *dir, size_t unused)
{
	int fd = OPEN(dir, O_RDONLY);

	struct pmem2_config cfg;
	init_cfg(&cfg, fd);

	size_t alignment;
	int ret = pmem2_config_get_alignment(&cfg, &alignment);

	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_INVALID_FILE_TYPE);

	CLOSE(fd);
}

static struct test_list {
	const char *name;
	test_fun test;
} list[] = {
	{"notset_fd", test_notset_fd},
	{"get_alignment_success", test_get_alignment_success},
	{"directory", test_directory},
};

int
main(int argc, char **argv)
{
	START(argc, argv, "pmem2_config_get_alignment");
	if (argc < 3 && argc > 4)
		UT_FATAL("usage: %s test_case path [alignment]", argv[0]);

	util_init();

	char *test_case = argv[1];
	char *path = argv[2];
	size_t ref_alignment = Ut_mmap_align;

	if (argc == 4)
		ref_alignment = ATOUL(argv[3]);

	for (int i = 0; i < ARRAY_SIZE(list); i++) {
		if (strcmp(list[i].name, test_case) == 0) {
			list[i].test(path, ref_alignment);
			goto end;
		}
	}
	UT_FATAL("test: %s doesn't exist", test_case);
end:
	DONE(NULL);
}

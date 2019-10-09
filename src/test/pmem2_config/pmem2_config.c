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
 * pmem_config.c -- pmem2_config unittests
 */
#include <libpmem2.h>
#include "fault_injection.h"
#include "unittest.h"
#include "ut_pmem2_config.h"


/*
 * test_cfg_alloc - test pmem2_config allocation
 */
static void
test_cfg_alloc(const char *unused)
{
	struct pmem2_config *cfg;
	PMEM2_CONFIG_NEW(&cfg);
	PMEM2_CONFIG_DELETE(&cfg);
}

/*
 * test_set_rw_fd - test setting O_RDWR fd
 */
static void
test_set_rw_fd(const char *file)
{
	struct pmem2_config *cfg;

	int fd = OPEN(file, O_RDWR);

	PMEM2_CONFIG_NEW(&cfg);
	PMEM2_CONFIG_SET_FD(cfg, fd);
	PMEM2_CONFIG_DELETE(&cfg);

	CLOSE(fd);
}

/*
 * test_set_ro_fd - test setting O_RDONLY fd
 */
static void
test_set_ro_fd(const char *file)
{
	struct pmem2_config *cfg;

	int fd = OPEN(file, O_RDONLY);

	PMEM2_CONFIG_NEW(&cfg);
	PMEM2_CONFIG_SET_FD(cfg, fd);
	PMEM2_CONFIG_DELETE(&cfg);

	CLOSE(fd);
}

/*
 * test_set_negative - test setting negative fd
 */
static void
test_set_negative_fd(const char *unused)
{
	struct pmem2_config *cfg;

	PMEM2_CONFIG_NEW(&cfg);
	/* randomly picked negative number */
	PMEM2_CONFIG_SET_FD(cfg, -42);
	PMEM2_CONFIG_DELETE(&cfg);
}

/*
 * test_set_invalid_fd - test setting invalid fd
 */
static void
test_set_invalid_fd(const char *file)
{
	struct pmem2_config *cfg;

	/* open and close the file to get invalid fd */
	int fd = OPEN(file, O_WRONLY);
	CLOSE(fd);

	PMEM2_CONFIG_NEW(&cfg);
	if (pmem2_config_set_fd(cfg, fd) != PMEM2_E_INVALID_ARG)
		UT_FATAL("pmem2_config_set_fd: %s", pmem2_errormsg());

	PMEM2_CONFIG_DELETE(&cfg);
}

/*
 * test_set_wronly_fd - test setting wronly fd
 */
static void
test_set_wronly_fd(const char *file)
{
	struct pmem2_config *cfg;

	int fd = OPEN(file, O_WRONLY);
	PMEM2_CONFIG_NEW(&cfg);
	if (pmem2_config_set_fd(cfg, fd) != PMEM2_E_INVALID_HANDLE)
		UT_FATAL("pmem2_config_set_fd: %s", pmem2_errormsg());

	PMEM2_CONFIG_DELETE(&cfg);
	CLOSE(fd);
}

/*
 * test_cfg_alloc_enomem - test pmem2_config allocation with error injection
 */
static void
test_alloc_cfg_enomem(const char *unused)
{
	struct pmem2_config *cfg;
	if (!common_fault_injection_enabled()) {
		return;
	}
	common_inject_fault_at(PMEM_MALLOC, 1, "Zalloc");
	if (pmem2_config_new(&cfg) != PMEM2_E_NOMEM)
		UT_FATAL("pmem2_config_new: %s", pmem2_errormsg());

	UT_ASSERTeq(cfg, NULL);
}

/*
 * test_delete_null_config - test pmem2_delete on NULL config
 */
static void
test_delete_null_config(const char *unused)
{
	struct pmem2_config *cfg = NULL;
	/* should not crash */
	PMEM2_CONFIG_DELETE(&cfg);
}

typedef void (*test_fun)(const char *file);

static struct test_list {
	const char *name;
	test_fun test;
} list[] = {
	{"alloc_cfg", test_cfg_alloc},
	{"set_rw_fd", test_set_rw_fd},
	{"set_ro_fd", test_set_ro_fd},
	{"set_negative_fd", test_set_negative_fd},
	{"set_invalid_fd", test_set_invalid_fd},
	{"set_wronly_fd", test_set_wronly_fd},
	{"alloc_cfg_enomem", test_alloc_cfg_enomem},
	{"delete_null_config", test_delete_null_config},
};

int
main(int argc, char **argv)
{
	START(argc, argv, "pmem2_config");
	if (argc != 3)
		UT_FATAL("usage: %s file test_case", argv[0]);

	char *file = argv[1];
	char *test_case = argv[2];

	for (int i = 0; i < ARRAY_SIZE(list); i++) {
		if (strcmp(list[i].name, test_case) == 0) {
			list[i].test(file);
			goto end;
		}
	}
	UT_FATAL("test: %s doesn't exist", test_case);
end:
	DONE(NULL);
}

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
#include "fault_injection.h"
#include "unittest.h"
#include "ut_pmem2_utils.h"
#include "ut_pmem2_config.h"
#include "pmem2.h"

/*
 * verify_fd -- verify value fd or handle in config
 */
static void
veryfy_fd(struct pmem2_config *cfg, int fd)
{
#ifdef WIN32
	UT_ASSERTeq(cfg->handle, fd != INVALID_FD ?
		(HANDLE)_get_osfhandle(fd) : INVALID_HANDLE_VALUE);
#else
	UT_ASSERTeq(cfg->fd, fd);
#endif
}

static struct pmem2_config Cfg;

/*
 * test_cfg_create_and_delete_valid - test pmem2_config allocation
 */
static void
test_cfg_create_and_delete_valid(const char *unused)
{
	struct pmem2_config *cfg_a;

	int ret = pmem2_config_new(&cfg_a);
	UT_PMEM2_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(cfg_a, NULL);
	veryfy_fd(cfg_a, INVALID_FD);
	ret = pmem2_config_delete(&cfg_a);
	UT_PMEM2_EXPECT_RETURN(ret, 0);
	UT_ASSERTeq(cfg_a, NULL);

}

/*
 * test_set_rw_fd - test setting O_RDWR fd
 */
static void
test_set_rw_fd(const char *file)
{
	int fd = OPEN(file, O_RDWR);

	int ret = pmem2_config_set_fd(&Cfg, fd);
	UT_PMEM2_EXPECT_RETURN(ret, 0);
	veryfy_fd(&Cfg, fd);

	CLOSE(fd);
}

/*
 * test_set_ro_fd - test setting O_RDONLY fd
 */
static void
test_set_ro_fd(const char *file)
{
	int fd = OPEN(file, O_RDONLY);

	int ret = pmem2_config_set_fd(&Cfg, fd);
	UT_PMEM2_EXPECT_RETURN(ret, 0);
	veryfy_fd(&Cfg, fd);

	CLOSE(fd);
}

/*
 * test_set_negative - test setting negative fd
 */
static void
test_set_negative_fd(const char *unused)
{
	/* randomly picked negative number */
	int ret = pmem2_config_set_fd(&Cfg, -42);
	UT_PMEM2_EXPECT_RETURN(ret, 0);
	veryfy_fd(&Cfg, INVALID_FD);
}

/*
 * test_set_invalid_fd - test setting invalid fd
 */
static void
test_set_invalid_fd(const char *file)
{
	/* open and close the file to get invalid fd */
	int fd = OPEN(file, O_WRONLY);
	CLOSE(fd);

	int ret = pmem2_config_set_fd(&Cfg, fd);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_INVALID_ARG);
	veryfy_fd(&Cfg, INVALID_FD);
}

/*
 * test_set_wronly_fd - test setting wronly fd
 */
static void
test_set_wronly_fd(const char *file)
{
	int fd = OPEN(file, O_WRONLY);

	int ret = pmem2_config_set_fd(&Cfg, fd);
#ifdef _WIN32
	/* windows doesn't validate open flags */
	UT_PMEM2_EXPECT_RETURN(ret, 0);
	veryfy_fd(&Cfg, fd);
#else
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_INVALID_HANDLE);
	veryfy_fd(&Cfg, INVALID_FD);
#endif
	CLOSE(fd);
}

/*
 * test_cfg_alloc_enomem - test pmem2_config allocation with error injection
 */
static void
test_alloc_cfg_enomem(const char *unused)
{
	struct pmem2_config *cfg_a;
	if (!common_fault_injection_enabled()) {
		return;
	}
	common_inject_fault_at(PMEM_MALLOC, 1, "Zalloc");

	int ret = pmem2_config_new(&cfg_a);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_NOMEM);

	UT_ASSERTeq(cfg_a, NULL);
}

/*
 * test_delete_null_config - test pmem2_delete on NULL config
 */
static void
test_delete_null_config(const char *unused)
{
	struct pmem2_config *null_cfg = NULL;
	/* should not crash */
	int ret = pmem2_config_delete(&null_cfg);
	UT_PMEM2_EXPECT_RETURN(ret, 0);
	UT_ASSERTeq(null_cfg, NULL);
}

typedef void (*test_fun)(const char *file);

static struct test_list {
	const char *name;
	test_fun test;
} list[] = {
	{"cfg_create_and_delete_valid", test_cfg_create_and_delete_valid},
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
		UT_FATAL("usage: %s test_case file", argv[0]);

	char *test_case = argv[1];
	char *file = argv[2];

	/* initialize static config */
#ifdef _WIN32
	Cfg.handle = INVALID_HANDLE_VALUE;
#else
	Cfg.fd = INVALID_FD;
#endif
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

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
#include "config.h"

/*
 * verify_fd -- verify value fd or handle in config
 */
static void
verify_fd(struct pmem2_config *cfg, int fd)
{
#ifdef WIN32
	UT_ASSERTeq(cfg->handle, fd != INVALID_FD ?
		(HANDLE)_get_osfhandle(fd) : INVALID_HANDLE_VALUE);
#else
	UT_ASSERTeq(cfg->fd, fd);
#endif
}

/*
 * test_cfg_create_and_delete_valid - test pmem2_config allocation
 */
static void
test_cfg_create_and_delete_valid(const char *unused)
{
	struct pmem2_config *cfg;

	int ret = pmem2_config_new(&cfg);
	UT_PMEM2_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(cfg, NULL);
	verify_fd(cfg, INVALID_FD);

	ret = pmem2_config_delete(&cfg);
	UT_PMEM2_EXPECT_RETURN(ret, 0);
	UT_ASSERTeq(cfg, NULL);
}

/*
 * test_set_rw_fd - test setting O_RDWR fd
 */
static void
test_set_rw_fd(const char *file)
{
	struct pmem2_config cfg;
	pmem2_config_init(&cfg);
	int fd = OPEN(file, O_RDWR);

	int ret = pmem2_config_set_fd(&cfg, fd);
	UT_PMEM2_EXPECT_RETURN(ret, 0);
	verify_fd(&cfg, fd);

	CLOSE(fd);
}

/*
 * test_set_ro_fd - test setting O_RDONLY fd
 */
static void
test_set_ro_fd(const char *file)
{
	struct pmem2_config cfg;
	pmem2_config_init(&cfg);
	int fd = OPEN(file, O_RDONLY);

	int ret = pmem2_config_set_fd(&cfg, fd);
	UT_PMEM2_EXPECT_RETURN(ret, 0);
	verify_fd(&cfg, fd);

	CLOSE(fd);
}

/*
 * test_set_negative - test setting negative fd
 */
static void
test_set_negative_fd(const char *unused)
{
	struct pmem2_config cfg;
	pmem2_config_init(&cfg);
	/* randomly picked negative number */
	int ret = pmem2_config_set_fd(&cfg, -42);
	UT_PMEM2_EXPECT_RETURN(ret, 0);
	verify_fd(&cfg, INVALID_FD);
}

/*
 * test_set_invalid_fd - test setting invalid fd
 */
static void
test_set_invalid_fd(const char *file)
{
	struct pmem2_config cfg;
	pmem2_config_init(&cfg);
	/* open and close the file to get invalid fd */
	int fd = OPEN(file, O_WRONLY);
	CLOSE(fd);
	ut_suppress_crt_assert();
	int ret = pmem2_config_set_fd(&cfg, fd);
	ut_unsuppress_crt_assert();
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_INVALID_FILE_HANDLE);
	verify_fd(&cfg, INVALID_FD);
}

/*
 * test_set_wronly_fd - test setting wronly fd
 */
static void
test_set_wronly_fd(const char *file)
{
	struct pmem2_config cfg;
	pmem2_config_init(&cfg);
	int fd = OPEN(file, O_WRONLY);

	int ret = pmem2_config_set_fd(&cfg, fd);
#ifdef _WIN32
	/* windows doesn't validate open flags */
	UT_PMEM2_EXPECT_RETURN(ret, 0);
	verify_fd(&cfg, fd);
#else
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_INVALID_FILE_HANDLE);
	verify_fd(&cfg, INVALID_FD);
#endif
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
	common_inject_fault_at(PMEM_MALLOC, 1, "pmem2_malloc");

	int ret = pmem2_config_new(&cfg);
	UT_PMEM2_EXPECT_RETURN(ret, -ENOMEM);

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
	int ret = pmem2_config_delete(&cfg);
	UT_PMEM2_EXPECT_RETURN(ret, 0);
	UT_ASSERTeq(cfg, NULL);
}

/*
 * test_config_set_granularity_valid - check valid granularity values
 */
static void
test_config_set_granularity_valid(const char *unused)
{
	struct pmem2_config cfg;
	pmem2_config_init(&cfg);

	/* check default granularity */
	enum pmem2_granularity g = PMEM2_GRANULARITY_INVALID;
	UT_ASSERTeq(cfg.requested_max_granularity, g);

	/* change default granularity */
	int ret = -1;
	g = PMEM2_GRANULARITY_BYTE;
	ret = pmem2_config_set_required_store_granularity(&cfg, g);
	UT_ASSERTeq(cfg.requested_max_granularity, g);
	UT_PMEM2_EXPECT_RETURN(ret, 0);

	/* set granularity once more */
	ret = -1;
	g = PMEM2_GRANULARITY_PAGE;
	ret = pmem2_config_set_required_store_granularity(&cfg, g);
	UT_ASSERTeq(cfg.requested_max_granularity, g);
	UT_PMEM2_EXPECT_RETURN(ret, 0);
}

/*
 * test_config_set_granularity_invalid - check invalid granularity values
 */
static void
test_config_set_granularity_invalid(const char *unused)
{
	/* pass invalid granularity */
	int ret = 0;
	enum pmem2_granularity g_inval = 999;
	struct pmem2_config cfg;
	pmem2_config_init(&cfg);
	ret = pmem2_config_set_required_store_granularity(&cfg, g_inval);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_INVALID_ARG);
}

#ifdef WIN32
/*
 * test_set_handle - test setting valid handle
 */
static void
test_set_handle(const char *file)
{
	struct pmem2_config cfg;
	pmem2_config_init(&cfg);
	HANDLE h = CreateFile(file, GENERIC_READ | GENERIC_WRITE,
		0, NULL, OPEN_ALWAYS, 0, NULL);
	UT_ASSERTne(h, INVALID_HANDLE_VALUE);

	int ret = pmem2_config_set_handle(&cfg, h);
	UT_PMEM2_EXPECT_RETURN(ret, 0);
	UT_ASSERTeq(cfg.handle, h);

	CloseHandle(h);
}

/*
 * test_set_null_handle - test resetting handle
 */
static void
test_set_null_handle(const char *unused)
{
	struct pmem2_config cfg;
	pmem2_config_init(&cfg);

	/* set the handle to something different than INVALID_HANDLE_VALUE */
	cfg.handle = NULL;

	int ret = pmem2_config_set_handle(&cfg, INVALID_HANDLE_VALUE);
	UT_PMEM2_EXPECT_RETURN(ret, 0);
	UT_ASSERTeq(cfg.handle, INVALID_HANDLE_VALUE);

}

/*
 * test_set_invalid_handle - test setting invalid handle
 */
static void
test_set_invalid_handle(const char *file)
{
	struct pmem2_config cfg;
	pmem2_config_init(&cfg);
	HANDLE h = CreateFile(file, GENERIC_READ | GENERIC_WRITE,
		0, NULL, OPEN_ALWAYS, 0, NULL);
	UT_ASSERTne(h, INVALID_HANDLE_VALUE);

	CloseHandle(h);

	int ret = pmem2_config_set_handle(&cfg, h);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_INVALID_FILE_HANDLE);
	UT_ASSERTeq(cfg.handle, INVALID_HANDLE_VALUE);
}

/*
 * test_set_directory_handle - test setting a directory handle
 */
static void
test_set_directory_handle(const char *file)
{
	struct pmem2_config cfg;
	pmem2_config_init(&cfg);

	HANDLE h = CreateFile(file, GENERIC_READ | GENERIC_WRITE,
		0, NULL, OPEN_ALWAYS, FILE_FLAG_BACKUP_SEMANTICS, NULL);

	UT_ASSERTne(h, INVALID_HANDLE_VALUE);

	int ret = pmem2_config_set_handle(&cfg, h);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_INVALID_FILE_TYPE);
	UT_ASSERTeq(cfg.handle, INVALID_HANDLE_VALUE);
	CloseHandle(h);
}

/*
 * test_set_directory_handle - test setting a mutex handle
 */
static void
test_set_mutex_handle(const char *unused)
{
	struct pmem2_config cfg;
	pmem2_config_init(&cfg);

	HANDLE h = CreateMutex(NULL, FALSE, NULL);
	UT_ASSERTne(h, INVALID_HANDLE_VALUE);

	int ret = pmem2_config_set_handle(&cfg, h);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_INVALID_FILE_HANDLE);
	UT_ASSERTeq(cfg.handle, INVALID_HANDLE_VALUE);
	CloseHandle(h);
}
#else
/*
 * test_set_directory_handle - test setting directory's fd
 */
static void
test_set_directory_fd(const char *file)
{
	struct pmem2_config cfg;
	pmem2_config_init(&cfg);

	int fd = OPEN(file, O_RDONLY);

	int ret = pmem2_config_set_fd(&cfg, fd);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_INVALID_FILE_TYPE);

	CLOSE(fd);
}
#endif

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
	{"config_set_granularity_valid", test_config_set_granularity_valid},
	{"config_set_granularity_invalid", test_config_set_granularity_invalid},
#ifdef _WIN32
	{"set_handle", test_set_handle},
	{"set_null_handle", test_set_null_handle},
	{"set_invalid_handle", test_set_invalid_handle},
	{"set_directory_handle", test_set_directory_handle},
	{"set_mutex_handle", test_set_mutex_handle},
#else
	{"set_directory_fd", test_set_directory_fd},
#endif
};

int
main(int argc, char **argv)
{
	START(argc, argv, "pmem2_config");
	if (argc != 3)
		UT_FATAL("usage: %s test_case file", argv[0]);

	char *test_case = argv[1];
	char *file = argv[2];

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

// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/*
 * pmem2_map_prot.c -- pmem2_map_prot unit tests
 */

#include <stdbool.h>
#include <signal.h>
#include <setjmp.h>

#include "config.h"
#include "source.h"
#include "map.h"
#include "out.h"
#include "pmem2.h"
#include "unittest.h"
#include "ut_pmem2.h"
#include "ut_pmem2_setup.h"
#include "ut_fh.h"

struct res {
	struct FHandle *fh;
	struct pmem2_config cfg;
	struct pmem2_source *src;
};

/*
 * res_prepare -- set access mode and protection flags
 */
static void
res_prepare(const char *file, struct res *res, int access, unsigned proto)
{
#ifdef _WIN32
	enum file_handle_type fh_type = FH_HANDLE;
#else
	enum file_handle_type fh_type = FH_FD;
#endif
	ut_pmem2_prepare_config(&res->cfg, &res->src, &res->fh, fh_type, file,
				0, 0, access);
	pmem2_config_set_protection(&res->cfg, proto);
}

/*
 * res_cleanup -- free resources
 */
static void
res_cleanup(struct res *res)
{
	PMEM2_SOURCE_DELETE(&res->src);
	UT_FH_CLOSE(res->fh);
}

static const char *word1 = "Persistent or nonpersistent: this is the question.";

static ut_jmp_buf_t Jmp;

/*
 * signal_handler -- called on SIGSEGV
 */
static void
signal_handler(int sig)
{
	ut_siglongjmp(Jmp);
}

/*
 * test_rw_mode_rw_prot -- test R/W protection
 * pmem2_map_new() - should success
 * memcpy() - should success
 */
static int
test_rw_mode_rw_prot(const struct test_case *tc,
		int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_rw_mode_rw_prot <file>");

	struct res res;

	/* read/write on file opened in read/write mode - should success */
	res_prepare(argv[0], &res, FH_RDWR,
			PMEM2_PROT_READ | PMEM2_PROT_WRITE);

	struct pmem2_map *map;
	int ret = pmem2_map_new(&map, &res.cfg, res.src);
	UT_ASSERTeq(ret, 0);

	pmem2_memcpy_fn memcpy_fn = pmem2_get_memcpy_fn(map);
	void *addr_map = pmem2_map_get_address(map);
	memcpy_fn(addr_map, word1, strlen(word1), 0);
	UT_ASSERTeq(memcmp(addr_map, word1, strlen(word1)), 0);

	pmem2_map_delete(&map);
	res_cleanup(&res);
	return 1;
}

/*
 * template_mode_prot_mismatch - try to map file with mutually exclusive FD
 * access and map protection
 */
static void
template_mode_prot_mismatch(char *file, int access, unsigned prot)
{
	struct res res;

	/* read/write on file opened in read-only mode - should fail */
	res_prepare(file, &res, access, prot);

	struct pmem2_map *map;
	int ret = pmem2_map_new(&map, &res.cfg, res.src);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_NO_ACCESS);

	res_cleanup(&res);
}

/*
 * test_r_mode_rw_prot -- test R/W protection
 * pmem2_map_new() - should fail
 */
static int
test_r_mode_rw_prot(const struct test_case *tc,
		int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_r_mode_rw_prot <file>");

	char *file = argv[0];
	template_mode_prot_mismatch(file, FH_READ,
					PMEM2_PROT_WRITE | PMEM2_PROT_READ);

	return 1;
}

/*
 * test_rw_mode_rwx_prot - test R/W/X protection on R/W file
 * pmem2_map_new() - should fail
 */
static int
test_rw_modex_rwx_prot(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_rw_modex_rwx_prot <file>");

	char *file = argv[0];
	template_mode_prot_mismatch(file, FH_RDWR,
			PMEM2_PROT_EXEC |PMEM2_PROT_WRITE | PMEM2_PROT_READ);

	return 1;
}

/*
 * test_rw_modex_rx_prot - test R/X protection on R/W file
 * pmem2_map_new() - should fail
 */
static int
test_rw_modex_rx_prot(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_rw_modex_rx_prot <file>");

	char *file = argv[0];
	template_mode_prot_mismatch(file, FH_RDWR,
					PMEM2_PROT_EXEC | PMEM2_PROT_READ);

	return 1;
}

/*
 * test_rw_mode_r_prot -- test R/W protection
 * pmem2_map_new() - should success
 * memcpy() - should fail
 */
static int
test_rw_mode_r_prot(const struct test_case *tc,
		int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_rw_mode_r_prot <file>");

	/* arrange to catch SIGSEGV */
	struct sigaction v;
	sigemptyset(&v.sa_mask);
	v.sa_flags = 0;
	v.sa_handler = signal_handler;
	SIGACTION(SIGSEGV, &v, NULL);

	struct res res;

	/* read-only on file opened in read/write mode - should success */
	res_prepare(argv[0], &res, FH_RDWR, PMEM2_PROT_READ);

	struct pmem2_map *map;
	int ret = pmem2_map_new(&map, &res.cfg, res.src);
	UT_ASSERTeq(ret, 0);

	pmem2_memcpy_fn memcpy_fn = pmem2_get_memcpy_fn(map);
	void *addr_map = pmem2_map_get_address(map);
	if (!ut_sigsetjmp(Jmp)) {
		/* memcpy should now fail */
		memcpy_fn(addr_map, word1, strlen(word1), 0);
		UT_FATAL("memcpy successful");
	}

	pmem2_map_delete(&map);
	res_cleanup(&res);
	signal(SIGSEGV, SIG_DFL);
	return 1;
}

/*
 * test_r_mode_r_prot -- test R/W protection
 * pmem2_map_new() - should success
 * memcpy() - should fail
 */
static int
test_r_mode_r_prot(const struct test_case *tc,
		int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_r_mode_r_prot <file>");

	/* arrange to catch SIGSEGV */
	struct sigaction v;
	sigemptyset(&v.sa_mask);
	v.sa_flags = 0;
	v.sa_handler = signal_handler;
	SIGACTION(SIGSEGV, &v, NULL);

	struct res res;

	/* read-only on file opened in read-only mode - should succeed */
	res_prepare(argv[0], &res, FH_READ, PMEM2_PROT_READ);

	struct pmem2_map *map;
	int ret = pmem2_map_new(&map, &res.cfg, res.src);
	UT_ASSERTeq(ret, 0);

	pmem2_memcpy_fn memcpy_fn = pmem2_get_memcpy_fn(map);
	void *addr_map = pmem2_map_get_address(map);
	if (!ut_sigsetjmp(Jmp)) {
		/* memcpy should now fail */
		memcpy_fn(addr_map, word1, strlen(word1), 0);
		UT_FATAL("memcpy successful");
	}

	pmem2_map_delete(&map);
	res_cleanup(&res);
	signal(SIGSEGV, SIG_DFL);
	return 1;
}

/*
 * test_rw_mode_none_prot -- test R/W protection
 * pmem2_map_new() - should success
 * memcpy() - should fail
 */
static int
test_rw_mode_none_prot(const struct test_case *tc,
		int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_rw_mode_none_prot <file>");

	/* arrange to catch SIGSEGV */
	struct sigaction v;
	sigemptyset(&v.sa_mask);
	v.sa_flags = 0;
	v.sa_handler = signal_handler;
	SIGACTION(SIGSEGV, &v, NULL);

	struct res res;

	/* none on file opened in read-only mode - should success */
	res_prepare(argv[0], &res, FH_READ, PMEM2_PROT_NONE);

	struct pmem2_map *map;
	int ret = pmem2_map_new(&map, &res.cfg, res.src);
	UT_ASSERTeq(ret, 0);

	pmem2_memcpy_fn memcpy_fn = pmem2_get_memcpy_fn(map);
	void *addr_map = pmem2_map_get_address(map);
	if (!ut_sigsetjmp(Jmp)) {
		/* memcpy should now fail */
		memcpy_fn(addr_map, word1, strlen(word1), 0);
		UT_FATAL("memcpy successful");
	}

	pmem2_map_delete(&map);
	res_cleanup(&res);
	signal(SIGSEGV, SIG_DFL);
	return 1;
}

/*
 * sum_asm[] --> simple program in assembly which calculates '2 + 2' and
 * returns the result
 */
static unsigned char sum_asm[] = {
0x55,						/* push	%rbp */
0x48, 0x89, 0xe5,				/* mov	%rsp,%rbp */
0xc7, 0x45, 0xf8, 0x02, 0x00, 0x00, 0x00,	/* movl	$0x2,-0x8(%rbp) */
0x8b, 0x45, 0xf8,				/* mov	-0x8(%rbp),%eax */
0x01, 0xc0,					/* add	%eax,%eax */
0x89, 0x45, 0xfc,				/* mov	%eax,-0x4(%rbp) */
0x8b, 0x45, 0xfc,				/* mov	-0x4(%rbp),%eax */
0x5d,						/* pop	%rbp */
0xc3,						/* retq */
};

typedef int (*sum_fn)(void);

/*
 * test_rx_mode_rx_prot_do_execute -- copy string with the program to mapped
 * memory to prepare memory, execute the program and verify result
 */
static int
test_rx_mode_rx_prot_do_execute(const struct test_case *tc, int argc,
				char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_rx_mode_rx_prot_do_execute <file>");

	char *file = argv[0];
	struct res res;

	/* Windows does not support PMEM2_PROT_WRITE combination */
	res_prepare(file, &res, FH_EXEC | FH_RDWR,
			PMEM2_PROT_WRITE | PMEM2_PROT_READ);

	struct pmem2_map *map;
	int ret = pmem2_map_new(&map, &res.cfg, res.src);
	UT_ASSERTeq(ret, 0);

	char *addr_map = pmem2_map_get_address(map);
	map->memcpy_fn(addr_map, sum_asm, sizeof(sum_asm), 0);

	pmem2_map_delete(&map);

	/* Windows does not support PMEM2_PROT_EXEC combination */
	pmem2_config_set_protection(&res.cfg,
					PMEM2_PROT_READ | PMEM2_PROT_EXEC);

	ret = pmem2_map_new(&map, &res.cfg, res.src);
	UT_ASSERTeq(ret, 0);

	sum_fn sum = (sum_fn)addr_map;
	int sum_result = sum();
	UT_ASSERTeq(sum_result, 4);

	pmem2_map_delete(&map);
	res_cleanup(&res);

	return 1;
}

/*
 * test_rwx_mode_rx_prot_do_write -- try to copy the string into mapped memory,
 * expect failure
 */
static int
test_rwx_mode_rx_prot_do_write(const struct test_case *tc,
		int argc, char *argv[])
{
	if (argc < 2)
		UT_FATAL(
		"usage: test_rwx_mode_rx_prot_do_write <file> <if_sharing>");

	struct sigaction v;
	sigemptyset(&v.sa_mask);
	v.sa_flags = 0;
	v.sa_handler = signal_handler;
	SIGACTION(SIGSEGV, &v, NULL);

	char *file = argv[0];
	unsigned if_sharing = ATOU(argv[1]);
	struct res res;

	/* Windows does not support PMEM2_PROT_EXEC combination */
	res_prepare(file, &res, FH_EXEC | FH_RDWR,
			PMEM2_PROT_READ | PMEM2_PROT_EXEC);

	if (if_sharing)
		pmem2_config_set_sharing(&res.cfg, PMEM2_PRIVATE);

	struct pmem2_map *map;
	int ret = pmem2_map_new(&map, &res.cfg, res.src);
	UT_ASSERTeq(ret, 0);

	char *addr_map = pmem2_map_get_address(map);
	if (!ut_sigsetjmp(Jmp)) {
		/* memcpy_fn should fail */
		map->memcpy_fn(addr_map, sum_asm, sizeof(sum_asm), 0);
	}

	pmem2_map_delete(&map);
	res_cleanup(&res);
	signal(SIGSEGV, SIG_DFL);

	return 2;
}

/*
 * test_rwx_mode_rwx_prot_do_execute -- copy string with the program to mapped
 * memory to prepare memory, execute the program and verify result
 */
static int
test_rwx_mode_rwx_prot_do_execute(const struct test_case *tc,
		int argc, char *argv[])
{
	if (argc < 2)
		UT_FATAL(
		"usage: test_rwx_mode_rwx_prot_do_execute <file> <if_sharing>");

	char *file = argv[0];
	unsigned if_sharing = ATOU(argv[1]);
	struct res res;

	res_prepare(file, &res, FH_EXEC | FH_RDWR,
			PMEM2_PROT_EXEC | PMEM2_PROT_WRITE | PMEM2_PROT_READ);

	if (if_sharing)
		pmem2_config_set_sharing(&res.cfg, PMEM2_PRIVATE);

	struct pmem2_map *map;
	int ret = pmem2_map_new(&map, &res.cfg, res.src);
	UT_ASSERTeq(ret, 0);

	char *addr_map = pmem2_map_get_address(map);
	map->memcpy_fn(addr_map, sum_asm, sizeof(sum_asm), 0);

	sum_fn sum = (sum_fn)addr_map;
	int sum_result = sum();
	UT_ASSERTeq(sum_result, 4);

	pmem2_map_delete(&map);
	res_cleanup(&res);
	signal(SIGSEGV, SIG_DFL);

	return 2;
}

/*
 * test_rw_mode_rw_prot_do_execute -- copy string with the program to mapped
 * memory to prepare memory, and execute the program - should fail
 */
static int
test_rw_mode_rw_prot_do_execute(const struct test_case *tc,
		int argc, char *argv[])
{
	if (argc < 2)
		UT_FATAL(
		"usage: test_rw_mode_rwx_prot_do_execute <file> <if_sharing>");

	struct sigaction v;
	sigemptyset(&v.sa_mask);
	v.sa_flags = 0;
	v.sa_handler = signal_handler;
	SIGACTION(SIGSEGV, &v, NULL);

	char *file = argv[0];
	unsigned if_sharing = ATOU(argv[1]);
	struct res res;

	res_prepare(file, &res, FH_RDWR, PMEM2_PROT_WRITE | PMEM2_PROT_READ);
	if (if_sharing)
		pmem2_config_set_sharing(&res.cfg, PMEM2_PRIVATE);

	struct pmem2_map *map;
	int ret = pmem2_map_new(&map, &res.cfg, res.src);
	UT_ASSERTeq(ret, 0);

	void *addr_map = pmem2_map_get_address(map);
	map->memcpy_fn(addr_map, sum_asm, sizeof(sum_asm), 0);

	sum_fn sum = (sum_fn)addr_map;
	if (!ut_sigsetjmp(Jmp)) {
		sum(); /* sum function should now fail */
	}

	pmem2_map_delete(&map);
	res_cleanup(&res);

	return 2;
}

static const char initial_state[] = "No code.";

/*
 * test_rwx_prot_map_priv_do_execute -- copy string with the program to
 * the mapped memory with MAP_PRIVATE to prepare memory, execute the program
 * and verify the result
 */
static int
test_rwx_prot_map_priv_do_execute(const struct test_case *tc,
	int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL(
		"usage: test_rwx_prot_map_priv_do_execute <file> <if_sharing>");

	char *file = argv[0];
	struct res res;

	res_prepare(file, &res, FH_RDWR, PMEM2_PROT_WRITE | PMEM2_PROT_READ);

	struct pmem2_map *map;
	int ret = pmem2_map_new(&map, &res.cfg, res.src);
	UT_ASSERTeq(ret, 0);

	char *addr_map = pmem2_map_get_address(map);
	map->memcpy_fn(addr_map, initial_state, sizeof(initial_state), 0);

	pmem2_map_delete(&map);
	res_cleanup(&res);

	res_prepare(file, &res, FH_READ | FH_EXEC,
			PMEM2_PROT_EXEC | PMEM2_PROT_WRITE | PMEM2_PROT_READ);
	pmem2_config_set_sharing(&res.cfg, PMEM2_PRIVATE);

	ret = pmem2_map_new(&map, &res.cfg, res.src);
	UT_ASSERTeq(ret, 0);

	addr_map = pmem2_map_get_address(map);
	map->memcpy_fn(addr_map, sum_asm, sizeof(sum_asm), 0);

	sum_fn sum = (sum_fn)addr_map;
	int sum_result = sum();
	UT_ASSERTeq(sum_result, 4);

	pmem2_map_delete(&map);

	ret = pmem2_map_new(&map, &res.cfg, res.src);
	UT_ASSERTeq(ret, 0);

	addr_map = pmem2_map_get_address(map);
	/* check if changes in private mapping affect initial state */
	UT_ASSERTeq(memcmp(addr_map, initial_state, strlen(initial_state)), 0);

	pmem2_map_delete(&map);
	res_cleanup(&res);

	return 1;
}

/*
 * test_cases -- available test cases
 */
static struct test_case test_cases[] = {
	TEST_CASE(test_rw_mode_rw_prot),
	TEST_CASE(test_r_mode_rw_prot),
	TEST_CASE(test_rw_modex_rwx_prot),
	TEST_CASE(test_rw_modex_rx_prot),
	TEST_CASE(test_rw_mode_r_prot),
	TEST_CASE(test_r_mode_r_prot),
	TEST_CASE(test_rw_mode_none_prot),
	TEST_CASE(test_rx_mode_rx_prot_do_execute),
	TEST_CASE(test_rwx_mode_rx_prot_do_write),
	TEST_CASE(test_rwx_mode_rwx_prot_do_execute),
	TEST_CASE(test_rw_mode_rw_prot_do_execute),
	TEST_CASE(test_rwx_prot_map_priv_do_execute),
};

#define NTESTS (sizeof(test_cases) / sizeof(test_cases[0]))

int
main(int argc, char *argv[])
{
	START(argc, argv, "pmem2_map_prot");
	TEST_CASE_PROCESS(argc, argv, test_cases, NTESTS);
	DONE(NULL);
}

#ifdef _MSC_VER
MSVC_CONSTR(libpmem2_init)
MSVC_DESTR(libpmem2_fini)
#endif

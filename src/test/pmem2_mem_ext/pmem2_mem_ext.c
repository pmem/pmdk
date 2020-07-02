// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/*
 * pmem2_mem_ext.c -- test for low level functions from libpmem2
 */

#include "unittest.h"
#include "file.h"
#include "ut_pmem2.h"
#include "valgrind_internal.h"

typedef void *(*memmove_fn)(void *pmemdest, const void *src, size_t len,
		unsigned flags);
typedef void *(*memcpy_fn)(void *pmemdest, const void *src, size_t len,
		unsigned flags);
typedef void *(*memset_fn)(void *pmemdest, int c, size_t len,
		unsigned flags);

static unsigned Flags[] = {
	0,
	PMEM_F_MEM_NONTEMPORAL,
	PMEM_F_MEM_TEMPORAL,
	PMEM_F_MEM_NONTEMPORAL | PMEM_F_MEM_TEMPORAL,
	PMEM_F_MEM_WC,
	PMEM_F_MEM_WB,
	PMEM_F_MEM_NOFLUSH,
	PMEM_F_MEM_NODRAIN | PMEM_F_MEM_NOFLUSH |
		PMEM_F_MEM_NONTEMPORAL | PMEM_F_MEM_TEMPORAL |
		PMEM_F_MEM_WC | PMEM_F_MEM_WB,
};

/*
 * do_memcpy_with_flag -- pmem2 memcpy with specified flag amd size
 */
static void
do_memcpy_with_flag(char *addr, size_t data_size, memcpy_fn cpy_fn, int flag)
{
	char *addr2 = addr + data_size;
	cpy_fn(addr2, addr, data_size, Flags[flag]);
}

/*
 * do_memmove_with_flag -- pmem2 memmove with specified flag and size
 */
static void
do_memmove_with_flag(char *addr, size_t data_size, memmove_fn mov_fn, int flag)
{
	char *addr2 = addr + data_size;
	mov_fn(addr2, addr, data_size, Flags[flag]);
}

/*
 * do_memset_with_flag -- pmem2 memset with specified flag and size
 */
static void
do_memset_with_flag(char *addr, size_t data_size, memset_fn set_fn, int flag)
{
	set_fn(addr, 1, data_size, Flags[flag]);
	if (Flags[flag] & PMEM2_F_MEM_NOFLUSH)
		VALGRIND_DO_PERSIST(addr, data_size);
}

int
main(int argc, char *argv[])
{
	int fd;
	char *addr;
	size_t mapped_len;
	struct pmem2_config *cfg;
	struct pmem2_source *src;
	struct pmem2_map *map;

	if (argc != 5)
		UT_FATAL("usage: %s file type size flag", argv[0]);

	const char *thr = os_getenv("PMEM_MOVNT_THRESHOLD");
	const char *avx = os_getenv("PMEM_AVX");
	const char *avx512f = os_getenv("PMEM_AVX512F");

	START(argc, argv, "pmem2_mem_ext %s %savx %savx512f",
			thr ? thr : "default",
			avx ? "" : "!",
			avx512f ? "" : "!");
	util_init();

	char type = argv[2][0];
	size_t data_size = strtoul(argv[3], NULL, 0);
	int flag = atoi(argv[4]);
	UT_ASSERT(flag < ARRAY_SIZE(Flags));

	fd = OPEN(argv[1], O_RDWR);
	UT_ASSERT(fd != -1);

	PMEM2_CONFIG_NEW(&cfg);
	PMEM2_SOURCE_FROM_FD(&src, fd);
	PMEM2_CONFIG_SET_GRANULARITY(cfg, PMEM2_GRANULARITY_PAGE);

	int ret = pmem2_map(&map, cfg, src);
	UT_PMEM2_EXPECT_RETURN(ret, 0);

	PMEM2_CONFIG_DELETE(&cfg);
	PMEM2_SOURCE_DELETE(&src);

	mapped_len = pmem2_map_get_size(map);
	UT_ASSERT(data_size * 2 < mapped_len);
	addr = pmem2_map_get_address(map);

	if (addr == NULL)
		UT_FATAL("!could not map file: %s", argv[1]);

	switch (type) {
	case 'C':
		{
			pmem2_memcpy_fn memcpy_fn = pmem2_get_memcpy_fn(map);
			do_memcpy_with_flag(addr, data_size, memcpy_fn, flag);
			break;
		}
	case 'S':
		{
			pmem2_memset_fn memset_fn = pmem2_get_memset_fn(map);
			do_memset_with_flag(addr, data_size, memset_fn, flag);
			break;
		}
	case 'M':
		{
			pmem2_memmove_fn memmove_fn = pmem2_get_memmove_fn(map);
			do_memmove_with_flag(addr, data_size, memmove_fn, flag);
			break;
		}
	default:
		UT_FATAL("!wrong type of test %c", type);
		break;
	}

	ret = pmem2_unmap(&map);
	UT_ASSERTeq(ret, 0);

	CLOSE(fd);

	DONE(NULL);
}

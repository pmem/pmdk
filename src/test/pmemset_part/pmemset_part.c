// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020-2021, Intel Corporation */

/*
 * pmemset_part.c -- pmemset_part unittests
 */

#include <string.h>
#ifndef _WIN32
#include <pthread.h>
#endif

#include "config.h"
#include "fault_injection.h"
#include "libpmemset.h"
#include "libpmem2.h"
#include "out.h"
#include "part.h"
#include "source.h"
#include "unittest.h"
#include "ut_pmemset_utils.h"

static void create_config(struct pmemset_config **cfg) {
	int ret = pmemset_config_new(cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(cfg, NULL);

	ret = pmemset_config_set_required_store_granularity(*cfg,
		PMEM2_GRANULARITY_PAGE);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(cfg, NULL);
}

/*
 * test_part_new_enomem - test pmemset_part allocation with error injection
 */
static int
test_part_new_enomem(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_part_new_enomem <path>");

	const char *file = argv[0];
	struct pmemset *set;
	struct pmemset_part *part;
	struct pmemset_source *src;
	struct pmemset_config *cfg;

	if (!core_fault_injection_enabled())
		return 1;

	create_config(&cfg);

	int ret = pmemset_new(&set, cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_source_from_file(&src, file);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(src, NULL);

	core_inject_fault_at(PMEM_MALLOC, 1, "pmemset_malloc");

	ret = pmemset_part_new(&part, set, src, 0, 0);
	UT_PMEMSET_EXPECT_RETURN(ret, -ENOMEM);
	UT_ASSERTeq(part, NULL);

	ret = pmemset_source_delete(&src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_delete(&set);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_config_delete(&cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	return 1;
}

/*
 * test_part_new_invalid_source_file - create a new part from a source
 *                                     with invalid path assigned
 */
static int
test_part_new_invalid_source_file(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_part_new_invalid_source_file <path>");

	const char *file = argv[0];
	struct pmemset *set;
	struct pmemset_source *src;
	struct pmemset_config *cfg;

	create_config(&cfg);

	int ret = pmemset_new(&set, cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_source_from_file(&src, file);
	UT_PMEMSET_EXPECT_RETURN(ret, PMEMSET_E_INVALID_SOURCE_PATH);

	ret = pmemset_source_delete(&src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_delete(&set);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_config_delete(&cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	return 1;
}

/*
 * test_part_new_valid_source_file - create a new part from a source
 *                                   with valid path assigned
 */
static int
test_part_new_valid_source_file(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_part_new_valid_source_file <path>");

	const char *file = argv[0];
	struct pmemset *set;
	struct pmemset_part *part;
	struct pmemset_source *src;
	struct pmemset_config *cfg;

	create_config(&cfg);

	int ret = pmemset_new(&set, cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_source_from_file(&src, file);
	UT_ASSERTeq(ret, 0);

	ret = pmemset_part_new(&part, set, src, 0, 0);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(part, NULL);

	ret = pmemset_part_delete(&part);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_source_delete(&src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_delete(&set);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_config_delete(&cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	return 1;
}

/*
 * test_part_new_valid_source_pmem2 - create a new part from a source
 *                                    with valid pmem2_source assigned
 */
static int
test_part_new_valid_source_pmem2(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_part_new_valid_source_pmem2 <path>");

	const char *file = argv[0];
	struct pmem2_source *pmem2_src;
	struct pmemset *set;
	struct pmemset_part *part;
	struct pmemset_source *src;
	struct pmemset_config *cfg;

	create_config(&cfg);

	int ret = pmemset_new(&set, cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	int fd = OPEN(file, O_RDWR);

	ret = pmem2_source_from_fd(&pmem2_src, fd);
	UT_ASSERTeq(ret, 0);

	ret = pmemset_source_from_pmem2(&src, pmem2_src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(src, NULL);

	ret = pmemset_part_new(&part, set, src, 0, 0);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(part, NULL);

	ret = pmemset_part_delete(&part);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_source_delete(&src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	CLOSE(fd);
	ret = pmemset_delete(&set);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_config_delete(&cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	return 1;
}

/*
 * test_part_map_valid_source_pmem2 - create a new part from a source
 *                                    with valid pmem2_source and map part
 */
static int
test_part_map_valid_source_pmem2(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_part_map_valid_source_pmem2 <path>");

	const char *file = argv[0];
	struct pmemset *set;
	struct pmemset_config *cfg;
	struct pmemset_part_descriptor desc;
	struct pmemset_part *part;
	struct pmemset_source *src;
	struct pmem2_source *pmem2_src;

	int fd = OPEN(file, O_RDWR);

	int ret = pmem2_source_from_fd(&pmem2_src, fd);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_source_from_pmem2(&src, pmem2_src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	create_config(&cfg);

	ret = pmemset_new(&set, cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_part_new(&part, set, src, 0, 64 * 1024);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_part_map(&part, NULL, &desc);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTeq(part, NULL);
	UT_ASSERTne(desc.addr, NULL);
	UT_ASSERTeq(desc.size, 64 * 1024);

	memset(desc.addr, 1, desc.size);

	ret = pmemset_delete(&set);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_config_delete(&cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_source_delete(&src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmem2_source_delete(&pmem2_src);
	UT_ASSERTeq(ret, 0);
	CLOSE(fd);

	return 1;
}

/*
 * test_part_map_valid_source_file - create a new part from a source
 *                                    with valid file path and map part
 */
static int
test_part_map_valid_source_file(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_part_map_valid_source_file <path>");

	const char *file = argv[0];
	struct pmemset_part *part;
	struct pmemset_source *src;
	struct pmemset *set;
	struct pmemset_config *cfg;

	int ret = pmemset_source_from_file(&src, file);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	create_config(&cfg);

	ret = pmemset_new(&set, cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_part_new(&part, set, src, 0, 64 * 1024);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_part_map(&part, NULL, NULL);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTeq(part, NULL);

	ret = pmemset_delete(&set);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_config_delete(&cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_source_delete(&src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	return 1;
}

/*
 * test_part_map_invalid_offset - create a new part from a source
 *                                    with invalid offset value
 */
static int
test_part_map_invalid_offset(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_part_map_invalid_offset <path>");

	const char *file = argv[0];
	struct pmemset_part *part;
	struct pmemset_source *src;
	struct pmemset *set;
	struct pmemset_config *cfg;

	int ret = pmemset_source_from_file(&src, file);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	create_config(&cfg);

	ret = pmemset_new(&set, cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_part_new(&part, set, src,
			(size_t)(INT64_MAX) + 1, 64 * 1024);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_part_map(&part, NULL, NULL);
	UT_PMEMSET_EXPECT_RETURN(ret, PMEMSET_E_INVALID_OFFSET_VALUE);

	ret = pmemset_part_delete(&part);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_delete(&set);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_config_delete(&cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_source_delete(&src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	return 1;
}

/*
 * test_part_map_gran_read - try to read effective granularity before
 * part mapping and after part mapping.
 */
static int
test_part_map_gran_read(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_part_map_gran_read <path>");

	const char *file = argv[0];
	struct pmemset_part *part;
	struct pmemset_source *src;
	struct pmemset *set;
	struct pmemset_config *cfg;
	enum pmem2_granularity effective_gran;

	int ret = pmemset_source_from_file(&src, file);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	create_config(&cfg);

	ret = pmemset_new(&set, cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_part_new(&part, set, src, 0, 64 * 1024);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_get_store_granularity(set, &effective_gran);
	UT_PMEMSET_EXPECT_RETURN(ret, PMEMSET_E_NO_PART_MAPPED);

	ret = pmemset_part_map(&part, NULL, NULL);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_get_store_granularity(set, &effective_gran);
	ASSERTeq(ret, 0);

	ret = pmemset_source_delete(&src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_config_delete(&cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_delete(&set);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	return 1;
}

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
 * test_unmap_part - test if data is unavailable after pmemset_delete
 */
static int
test_unmap_part(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_part_map_invalid_offset <path>");

	const char *file = argv[0];
	struct pmemset_part *part;
	struct pmemset_source *src;
	struct pmemset *set;
	struct pmemset_config *cfg;

	int ret = pmemset_source_from_file(&src, file);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	create_config(&cfg);

	ret = pmemset_new(&set, cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_part_new(&part, set, src, 0, 0);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	struct pmemset_part_descriptor desc;
	ret = pmemset_part_map(&part, NULL, &desc);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTeq(part, NULL);

	memset(desc.addr, 1, desc.size);
	pmemset_persist(set, desc.addr, desc.size);

	ret = pmemset_delete(&set);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	struct sigaction v;
	sigemptyset(&v.sa_mask);
	v.sa_flags = 0;
	v.sa_handler = signal_handler;
	SIGACTION(SIGSEGV, &v, NULL);
	if (!ut_sigsetjmp(Jmp)) {
		/* memcpy should now fail */
		memset(desc.addr, 1, desc.size);
		UT_FATAL("memcpy successful");
	}
	signal(SIGSEGV, SIG_DFL);

	ret = pmemset_config_delete(&cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_source_delete(&src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	return 1;
}

/*
 * test_part_map_enomem - test pmemset_part_map allocation with error injection
 */
static int
test_part_map_enomem(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_part_map_enomem <path>");

	const char *file = argv[0];
	struct pmemset *set;
	struct pmemset_part *part;
	struct pmemset_source *src;
	struct pmemset_config *cfg;

	if (!core_fault_injection_enabled())
		return 1;

	create_config(&cfg);

	int ret = pmemset_new(&set, cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_source_from_file(&src, file);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(src, NULL);

	ret = pmemset_part_new(&part, set, src, 0, 0);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	core_inject_fault_at(PMEM_MALLOC, 1, "pmemset_malloc");
	ret = pmemset_part_map(&part, NULL, NULL);
	UT_PMEMSET_EXPECT_RETURN(ret, -ENOMEM);

	ret = pmemset_part_delete(&part);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_source_delete(&src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_delete(&set);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_config_delete(&cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	return 1;
}

/*
 * test_part_map_first - get the first (earliest in the memory)
 *                       mapping from the set
 */
static int
test_part_map_first(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_part_map_first <path>");

	const char *file = argv[0];
	struct pmem2_source *pmem2_src;
	struct pmemset *set;
	struct pmemset_config *cfg;
	struct pmemset_part *part;
	struct pmemset_part_map *first_pmap = NULL;
	struct pmemset_source *src;
	size_t part_size = 64 * 1024;

	int fd = OPEN(file, O_RDWR);

	int ret = pmem2_source_from_fd(&pmem2_src, fd);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_source_from_pmem2(&src, pmem2_src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	create_config(&cfg);

	ret = pmemset_new(&set, cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_part_new(&part, set, src, 0, part_size);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_part_map(&part, NULL, NULL);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	pmemset_first_part_map(set, &first_pmap);
	UT_ASSERTne(first_pmap, NULL);

	ret = pmemset_delete(&set);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_config_delete(&cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_source_delete(&src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmem2_source_delete(&pmem2_src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	CLOSE(fd);

	return 1;
}

/*
 * test_part_map_descriptor - test retrieving first (earliest in the memory)
 *                            mapping from the set
 */
static int
test_part_map_descriptor(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_part_map_descriptor <path>");

	const char *file = argv[0];
	struct pmem2_source *pmem2_src;
	struct pmemset *set;
	struct pmemset_config *cfg;
	struct pmemset_part *part;
	struct pmemset_part_descriptor desc;
	struct pmemset_part_map *first_pmap = NULL;
	struct pmemset_source *src;
	size_t part_size = 64 * 1024;

	int fd = OPEN(file, O_RDWR);

	int ret = pmem2_source_from_fd(&pmem2_src, fd);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_source_from_pmem2(&src, pmem2_src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	create_config(&cfg);

	ret = pmemset_new(&set, cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_part_new(&part, set, src, 0, part_size);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_part_map(&part, NULL, NULL);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	pmemset_first_part_map(set, &first_pmap);
	UT_ASSERTne(first_pmap, NULL);

	desc = pmemset_descriptor_part_map(first_pmap);
	UT_ASSERTne(desc.addr, NULL);
	UT_ASSERTeq(desc.size, part_size);

	ret = pmemset_delete(&set);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_config_delete(&cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_source_delete(&src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmem2_source_delete(&pmem2_src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	CLOSE(fd);

	return 1;
}

/*
 * test_part_map_next - test retrieving next mapping from the set
 */
static int
test_part_map_next(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_part_map_next <path>");

	const char *file = argv[0];
	struct pmem2_source *pmem2_src;
	struct pmemset *set;
	struct pmemset_config *cfg;
	struct pmemset_part_descriptor first_desc;
	struct pmemset_part_descriptor second_desc;
	struct pmemset_part *part;
	struct pmemset_part_map *first_pmap = NULL;
	struct pmemset_part_map *second_pmap = NULL;
	struct pmemset_source *src;
	size_t first_part_size = 64 * 1024;
	size_t second_part_size = 128 * 1024;

	int fd = OPEN(file, O_RDWR);

	int ret = pmem2_source_from_fd(&pmem2_src, fd);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_source_from_pmem2(&src, pmem2_src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	create_config(&cfg);

	ret = pmemset_new(&set, cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_part_new(&part, set, src, 0, first_part_size);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_part_map(&part, NULL, NULL);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_part_new(&part, set, src, 0, second_part_size);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_part_map(&part, NULL, NULL);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	pmemset_first_part_map(set, &first_pmap);
	UT_ASSERTne(first_pmap, NULL);

	pmemset_next_part_map(set, first_pmap, &second_pmap);
	UT_ASSERTne(second_pmap, NULL);

	first_desc = pmemset_descriptor_part_map(first_pmap);
	second_desc = pmemset_descriptor_part_map(second_pmap);
	/*
	 * we don't know which mapping is first, but we know that the first
	 * mapping should be mapped lower than its successor
	 */
	UT_ASSERT(first_desc.addr < second_desc.addr);
	UT_ASSERTne(first_desc.size, second_desc.size);

	ret = pmemset_delete(&set);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_config_delete(&cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_source_delete(&src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmem2_source_delete(&pmem2_src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	CLOSE(fd);

	return 1;
}

/*
 * test_part_map_drop - test dropping the access to the pointer obtained from
 *                      set iterator
 */
static int
test_part_map_drop(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_part_map_drop <path>");

	const char *file = argv[0];
	struct pmem2_source *pmem2_src;
	struct pmemset *set;
	struct pmemset_config *cfg;
	struct pmemset_part *part;
	struct pmemset_part_map *pmap = NULL;
	struct pmemset_source *src;
	size_t part_size = 64 * 1024;

	int fd = OPEN(file, O_RDWR);

	int ret = pmem2_source_from_fd(&pmem2_src, fd);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_source_from_pmem2(&src, pmem2_src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	create_config(&cfg);

	ret = pmemset_new(&set, cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_part_new(&part, set, src, 0, part_size);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_part_map(&part, NULL, NULL);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	pmemset_first_part_map(set, &pmap);
	UT_ASSERTne(pmap, NULL);

	pmemset_part_map_drop(&pmap);
	UT_ASSERTeq(pmap, NULL);

	ret = pmemset_delete(&set);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_config_delete(&cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_source_delete(&src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmem2_source_delete(&pmem2_src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	CLOSE(fd);

	return 1;
}

/*
 * test_part_map_by_addr -- reads part map by passed address
 */
static int
test_part_map_by_addr(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_part_map_by_addr <path>");

	const char *file = argv[0];
	struct pmem2_source *pmem2_src;
	struct pmemset *set;
	struct pmemset_config *cfg;
	struct pmemset_part_descriptor first_desc;
	struct pmemset_part_descriptor second_desc;
	struct pmemset_part_descriptor first_desc_ba;
	struct pmemset_part_descriptor second_desc_ba;
	struct pmemset_part *part;
	struct pmemset_part_map *first_pmap = NULL;
	struct pmemset_part_map *second_pmap = NULL;
	struct pmemset_part_map *first_pmap_ba = NULL;
	struct pmemset_part_map *second_pmap_ba = NULL;
	struct pmemset_source *src;
	size_t part_size_first = 64 * 1024;
	size_t part_size_second = 128 * 1024;

	int fd = OPEN(file, O_RDWR);

	int ret = pmem2_source_from_fd(&pmem2_src, fd);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_source_from_pmem2(&src, pmem2_src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	create_config(&cfg);

	ret = pmemset_new(&set, cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_part_new(&part, set, src, 0, part_size_first);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_part_map(&part, NULL, NULL);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_part_new(&part, set, src, 0, part_size_second);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_part_map(&part, NULL, NULL);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	pmemset_first_part_map(set, &first_pmap);
	UT_ASSERTne(first_pmap, NULL);

	pmemset_next_part_map(set, first_pmap, &second_pmap);
	UT_ASSERTne(second_pmap, NULL);

	first_desc = pmemset_descriptor_part_map(first_pmap);
	second_desc = pmemset_descriptor_part_map(second_pmap);

	ret = pmemset_part_map_by_address(set, &first_pmap_ba, first_desc.addr);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_part_map_by_address(set, &second_pmap_ba,
			second_desc.addr);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	first_desc_ba = pmemset_descriptor_part_map(first_pmap_ba);
	second_desc_ba = pmemset_descriptor_part_map(second_pmap_ba);

	UT_ASSERTne(first_desc_ba.addr, second_desc_ba.addr);
	UT_ASSERTeq(first_desc_ba.addr, first_desc.addr);
	UT_ASSERTne(first_desc.size, second_desc.size);

	ret = pmemset_part_map_by_address(set, &first_pmap_ba, (void *)0x999);
	UT_PMEMSET_EXPECT_RETURN(ret, PMEMSET_E_CANNOT_FIND_PART_MAP);

	pmemset_config_delete(&cfg);
	pmem2_source_delete(&pmem2_src);
	CLOSE(fd);

	return 1;
}

/*
 * test_part_map_unaligned_size - create a new part from file with
 *                                unaligned size
 */
static int
test_part_map_unaligned_size(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_part_map_unaligned_size <path>");

	const char *file = argv[0];
	struct pmemset *set;
	struct pmemset_config *cfg;
	struct pmemset_part *part;
	struct pmemset_source *src;
	struct pmem2_source *pmem2_src;

	int fd = OPEN(file, O_RDWR);

	int ret = pmem2_source_from_fd(&pmem2_src, fd);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_source_from_pmem2(&src, pmem2_src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	create_config(&cfg);

	ret = pmemset_new(&set, cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_part_new(&part, set, src, 0, 0);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_part_map(&part, NULL, NULL);
	UT_PMEMSET_EXPECT_RETURN(ret, PMEMSET_E_LENGTH_UNALIGNED);

	ret = pmemset_part_delete(&part);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_delete(&set);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_config_delete(&cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_source_delete(&src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmem2_source_delete(&pmem2_src);
	UT_ASSERTeq(ret, 0);
	CLOSE(fd);

	return 1;
}

/*
 * test_part_map_full_coalesce_before - turn on full coalescing feature then
 *                                      create two mappings
 */
static int
test_part_map_full_coalesce_before(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_part_map_coalesce_before <path>");

	const char *file = argv[0];
	struct pmem2_source *pmem2_src;
	struct pmemset *set;
	struct pmemset_config *cfg;
	struct pmemset_part_descriptor desc_before;
	struct pmemset_part_descriptor desc_after;
	struct pmemset_part *part;
	struct pmemset_part_map *first_pmap = NULL;
	struct pmemset_part_map *second_pmap = NULL;
	struct pmemset_source *src;

	int fd = OPEN(file, O_RDWR);

	int ret = pmem2_source_from_fd(&pmem2_src, fd);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_source_from_pmem2(&src, pmem2_src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	create_config(&cfg);

	ret = pmemset_new(&set, cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_set_contiguous_part_coalescing(set,
			PMEMSET_COALESCING_FULL);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_part_new(&part, set, src, 0, 0);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_part_map(&part, NULL, NULL);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	pmemset_first_part_map(set, &first_pmap);
	UT_ASSERTne(first_pmap, NULL);

	desc_before = pmemset_descriptor_part_map(first_pmap);

	ret = pmemset_part_new(&part, set, src, 0, 0);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_part_map(&part, NULL, NULL);
	if (ret == PMEMSET_E_CANNOT_COALESCE_PARTS)
		goto err_cleanup;
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	/* when full coalescing is on, parts should become one part mapping */
	pmemset_first_part_map(set, &first_pmap);
	UT_ASSERTne(first_pmap, NULL);

	pmemset_next_part_map(set, first_pmap, &second_pmap);
	UT_ASSERTeq(second_pmap, NULL);

	desc_after = pmemset_descriptor_part_map(first_pmap);

	UT_ASSERTeq(desc_before.addr, desc_after.addr);
	UT_ASSERT(desc_before.size < desc_after.size);

err_cleanup:
	ret = pmemset_delete(&set);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_config_delete(&cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_source_delete(&src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmem2_source_delete(&pmem2_src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	CLOSE(fd);

	return 1;
}

/*
 * test_part_map_full_coalesce_after - map a part, turn on full coalescing
 *                                     feature and map a part second time
 */
static int
test_part_map_full_coalesce_after(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_part_map_full_coalesce_after <path>");

	const char *file = argv[0];
	struct pmem2_source *pmem2_src;
	struct pmemset *set;
	struct pmemset_config *cfg;
	struct pmemset_part_descriptor desc_before;
	struct pmemset_part_descriptor desc_after;
	struct pmemset_part *part;
	struct pmemset_part_map *first_pmap = NULL;
	struct pmemset_part_map *second_pmap = NULL;
	struct pmemset_source *src;

	int fd = OPEN(file, O_RDWR);

	int ret = pmem2_source_from_fd(&pmem2_src, fd);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_source_from_pmem2(&src, pmem2_src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	create_config(&cfg);

	ret = pmemset_new(&set, cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_part_new(&part, set, src, 0, 0);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_part_map(&part, NULL, NULL);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	pmemset_first_part_map(set, &first_pmap);
	UT_ASSERTne(first_pmap, NULL);

	desc_before = pmemset_descriptor_part_map(first_pmap);

	ret = pmemset_set_contiguous_part_coalescing(set,
			PMEMSET_COALESCING_FULL);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_part_new(&part, set, src, 0, 0);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_part_map(&part, NULL, NULL);
	if (ret == PMEMSET_E_CANNOT_COALESCE_PARTS)
		goto err_cleanup;
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	/* when full coalescing is on, parts should become one part mapping */
	pmemset_first_part_map(set, &first_pmap);
	UT_ASSERTne(first_pmap, NULL);

	pmemset_next_part_map(set, first_pmap, &second_pmap);
	UT_ASSERTeq(second_pmap, NULL);

	desc_after = pmemset_descriptor_part_map(first_pmap);

	UT_ASSERTeq(desc_before.addr, desc_after.addr);
	UT_ASSERT(desc_before.size < desc_after.size);

err_cleanup:
	ret = pmemset_delete(&set);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_config_delete(&cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_source_delete(&src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmem2_source_delete(&pmem2_src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	CLOSE(fd);

	return 1;
}

/*
 * test_part_map_opp_coalesce_before - turn on opportunistic coalescing feature
 *                                     then create two mappings
 */
static int
test_part_map_opp_coalesce_before(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_part_map_opp_coalesce_before <path>");

	const char *file = argv[0];
	struct pmem2_source *pmem2_src;
	struct pmemset *set;
	struct pmemset_config *cfg;
	struct pmemset_part_descriptor desc_first;
	struct pmemset_part_descriptor desc_second;
	struct pmemset_part *part;
	struct pmemset_part_map *first_pmap = NULL;
	struct pmemset_part_map *second_pmap = NULL;
	struct pmemset_source *src;
	size_t part_size = 64 * 1024;

	int fd = OPEN(file, O_RDWR);

	int ret = pmem2_source_from_fd(&pmem2_src, fd);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_source_from_pmem2(&src, pmem2_src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	create_config(&cfg);

	ret = pmemset_new(&set, cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_set_contiguous_part_coalescing(set,
			PMEMSET_COALESCING_OPPORTUNISTIC);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_part_new(&part, set, src, 0, part_size);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_part_map(&part, NULL, NULL);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_part_new(&part, set, src, 0, part_size);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_part_map(&part, NULL, NULL);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	/*
	 * when opportunistic coalescing is on, parts could
	 * either be coalesced or mapped apart
	 */
	pmemset_first_part_map(set, &first_pmap);
	UT_ASSERTne(first_pmap, NULL);
	desc_first = pmemset_descriptor_part_map(first_pmap);

	pmemset_next_part_map(set, first_pmap, &second_pmap);
	if (second_pmap) {
		desc_second = pmemset_descriptor_part_map(second_pmap);
		UT_ASSERTeq(desc_first.size, desc_second.size);
	} else {
		UT_ASSERTeq(desc_first.size, 2 * part_size);
	}

	ret = pmemset_delete(&set);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_config_delete(&cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_source_delete(&src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmem2_source_delete(&pmem2_src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	CLOSE(fd);

	return 1;
}

/*
 * test_part_map_opp_coalesce_after - map a part, turn on opportunistic
 * coalescing feature and map a part second time.
 */
static int
test_part_map_opp_coalesce_after(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_part_map_opp_coalesce_after <path>");

	const char *file = argv[0];
	struct pmem2_source *pmem2_src;
	struct pmemset *set;
	struct pmemset_config *cfg;
	struct pmemset_part_descriptor desc_first;
	struct pmemset_part_descriptor desc_second;
	struct pmemset_part *part;
	struct pmemset_part_map *first_pmap = NULL;
	struct pmemset_part_map *second_pmap = NULL;
	struct pmemset_source *src;
	size_t part_size = 64 * 1024;

	int fd = OPEN(file, O_RDWR);

	int ret = pmem2_source_from_fd(&pmem2_src, fd);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_source_from_pmem2(&src, pmem2_src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	create_config(&cfg);

	ret = pmemset_new(&set, cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_part_new(&part, set, src, 0, part_size);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_part_map(&part, NULL, NULL);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_set_contiguous_part_coalescing(set,
			PMEMSET_COALESCING_OPPORTUNISTIC);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_part_new(&part, set, src, 0, part_size);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_part_map(&part, NULL, NULL);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	/*
	 * when opportunistic coalescing is on, parts could
	 * either be coalesced or mapped apart
	 */
	pmemset_first_part_map(set, &first_pmap);
	UT_ASSERTne(first_pmap, NULL);
	desc_first = pmemset_descriptor_part_map(first_pmap);

	pmemset_next_part_map(set, first_pmap, &second_pmap);
	if (second_pmap) {
		desc_second = pmemset_descriptor_part_map(second_pmap);
		UT_ASSERTeq(desc_first.size, desc_second.size);
	} else {
		UT_ASSERTeq(desc_first.size, 2 * part_size);
	}

	ret = pmemset_delete(&set);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_config_delete(&cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_source_delete(&src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmem2_source_delete(&pmem2_src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	CLOSE(fd);

	return 1;
}

/*
 * test_remove_part_map -- map two parts to the pmemset, iterate over
 *                         the set to find them, then remove them
 */
static int
test_remove_part_map(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_part_map_drop <path>");

	const char *file = argv[0];
	struct pmem2_source *pmem2_src;
	struct pmemset *set;
	struct pmemset_config *cfg;
	struct pmemset_part_descriptor first_desc;
	struct pmemset_part_descriptor second_desc;
	struct pmemset_part *part;
	struct pmemset_part_map *first_pmap = NULL;
	struct pmemset_part_map *second_pmap = NULL;
	struct pmemset_source *src;
	size_t first_part_size = 64 * 1024;
	size_t second_part_size = 128 * 1024;

	int fd = OPEN(file, O_RDWR);

	int ret = pmem2_source_from_fd(&pmem2_src, fd);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_source_from_pmem2(&src, pmem2_src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	create_config(&cfg);

	ret = pmemset_new(&set, cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_part_new(&part, set, src, 0, first_part_size);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_part_map(&part, NULL, NULL);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_part_new(&part, set, src, 0, second_part_size);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_part_map(&part, NULL, NULL);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	pmemset_first_part_map(set, &first_pmap);
	UT_ASSERTne(first_pmap, NULL);

	pmemset_next_part_map(set, first_pmap, &second_pmap);
	UT_ASSERTne(second_pmap, NULL);
	first_desc = pmemset_descriptor_part_map(second_pmap);

	pmemset_part_map_drop(&second_pmap);

	/*
	 * after removing first mapped part pmemset should contain
	 * only the part that was mapped second
	 */
	ret = pmemset_remove_part_map(set, &first_pmap);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(first_pmap, NULL);

	pmemset_first_part_map(set, &first_pmap);
	UT_ASSERTne(first_pmap, 0);
	second_desc = pmemset_descriptor_part_map(first_pmap);
	UT_ASSERTeq(first_desc.addr, second_desc.addr);

	ret = pmemset_remove_part_map(set, &first_pmap);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(first_pmap, NULL);

	/* after removing second mapped part, pmemset should be empty */
	pmemset_first_part_map(set, &first_pmap);
	UT_ASSERTeq(first_pmap, NULL);

	ret = pmemset_delete(&set);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_config_delete(&cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_source_delete(&src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmem2_source_delete(&pmem2_src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	CLOSE(fd);

	return 1;
}

/*
 * test_full_coalescing_before_remove_part_map -- enable the part
 * coalescing feature map two parts to the pmemset. If no error returned those
 * parts should appear as single part mapping. Iterate over the set to obtain
 * coalesced part mapping and delete it.
 */
static int
test_full_coalescing_before_remove_part_map(const struct test_case *tc,
		int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL(
			"usage: test_full_coalescing_before_remove_part_map" \
			"<path>");

	const char *file = argv[0];
	struct pmem2_source *pmem2_src;
	struct pmemset *set;
	struct pmemset_config *cfg;
	struct pmemset_part *part;
	struct pmemset_part_map *first_pmap = NULL;
	struct pmemset_part_map *second_pmap = NULL;
	struct pmemset_source *src;
	size_t first_part_size = 64 * 1024;
	size_t second_part_size = 128 * 1024;

	int fd = OPEN(file, O_RDWR);

	int ret = pmem2_source_from_fd(&pmem2_src, fd);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_source_from_pmem2(&src, pmem2_src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	create_config(&cfg);

	ret = pmemset_new(&set, cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_set_contiguous_part_coalescing(set,
			PMEMSET_COALESCING_FULL);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_part_new(&part, set, src, 0, first_part_size);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_part_map(&part, NULL, NULL);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_part_new(&part, set, src, 0, second_part_size);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_part_map(&part, NULL, NULL);
	/* the address next to the previous part mapping is already occupied */
	if (ret == PMEMSET_E_CANNOT_COALESCE_PARTS) {
		pmemset_part_delete(&part);
		goto err_cleanup;
	}
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	pmemset_first_part_map(set, &first_pmap);
	UT_ASSERTne(first_pmap, NULL);

	pmemset_next_part_map(set, first_pmap, &second_pmap);
	UT_ASSERTeq(second_pmap, NULL);

	/*
	 * after removing first mapped part,
	 * pmemset should contain only the part that was mapped second
	 */
	ret = pmemset_remove_part_map(set, &first_pmap);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(first_pmap, NULL);

	/* after removing coalesced mapped part, pmemset should be empty */
	pmemset_first_part_map(set, &first_pmap);
	UT_ASSERTeq(first_pmap, NULL);

err_cleanup:
	ret = pmemset_delete(&set);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_config_delete(&cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_source_delete(&src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmem2_source_delete(&pmem2_src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	CLOSE(fd);

	return 1;
}

/*
 * test_full_coalescing_after_remove_first_part_map -- map two parts to the
 * pmemset, iterate over the set to find first mapping and delete it. Turn on
 * part coalescing and map new part. Lastly iterate over the set to find the
 * coalesced mapping and delete it.
 */
static int
test_full_coalescing_after_remove_first_part_map(const struct test_case *tc,
		int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL(
			"usage: test_full_coalescing_after_remove_first_part_map" \
			"<path>");

	const char *file = argv[0];
	struct pmem2_source *pmem2_src;
	struct pmemset *set;
	struct pmemset_config *cfg;
	struct pmemset_part *part;
	struct pmemset_part_map *first_pmap = NULL;
	struct pmemset_part_map *second_pmap = NULL;
	struct pmemset_source *src;

	int fd = OPEN(file, O_RDWR);

	int ret = pmem2_source_from_fd(&pmem2_src, fd);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_source_from_pmem2(&src, pmem2_src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	create_config(&cfg);

	ret = pmemset_new(&set, cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	/* map first part */
	ret = pmemset_part_new(&part, set, src, 0, 0);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_part_map(&part, NULL, NULL);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	/* map second part */
	ret = pmemset_part_new(&part, set, src, 0, 0);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_part_map(&part, NULL, NULL);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	/* there should be two mapping in the pmemset */
	pmemset_first_part_map(set, &first_pmap);
	UT_ASSERTne(first_pmap, NULL);

	pmemset_next_part_map(set, first_pmap, &second_pmap);
	UT_ASSERTne(second_pmap, NULL);

	struct pmemset_part_descriptor desc_before;
	desc_before = pmemset_descriptor_part_map(second_pmap);

	pmemset_part_map_drop(&second_pmap);

	/* delete first mapping */
	ret = pmemset_remove_part_map(set, &first_pmap);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(first_pmap, NULL);

	pmemset_first_part_map(set, &first_pmap);
	UT_ASSERTne(first_pmap, NULL);

	/* check if the second mapped part is still kept in pmemset */
	struct pmemset_part_descriptor desc_after;
	desc_after = pmemset_descriptor_part_map(first_pmap);
	UT_ASSERTeq(desc_before.addr, desc_after.addr);

	pmemset_part_map_drop(&first_pmap);

	/* turn on coalescing */
	ret = pmemset_set_contiguous_part_coalescing(set,
			PMEMSET_COALESCING_FULL);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	/* new part should be coalesced with the remaining mapping */
	ret = pmemset_part_new(&part, set, src, 0, 0);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_part_map(&part, NULL, NULL);
	/* the address next to the previous part mapping is already occupied */
	if (ret == PMEMSET_E_CANNOT_COALESCE_PARTS) {
		pmemset_part_delete(&part);
		goto err_cleanup;
	}
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	/* coalesced part mapping should be the only mapping in the pmemset */
	pmemset_first_part_map(set, &first_pmap);
	UT_ASSERTne(first_pmap, NULL);

	pmemset_next_part_map(set, first_pmap, &second_pmap);
	UT_ASSERTeq(second_pmap, NULL);

	ret = pmemset_remove_part_map(set, &first_pmap);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(first_pmap, NULL);

	/* after removing coalesced mapped part, pmemset should be empty */
	pmemset_first_part_map(set, &first_pmap);
	UT_ASSERTeq(first_pmap, NULL);

err_cleanup:
	ret = pmemset_delete(&set);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_config_delete(&cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_source_delete(&src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmem2_source_delete(&pmem2_src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	CLOSE(fd);

	return 1;
}

/*
 * test_full_coalescing_after_remove_second_part_map -- map two parts to the
 * pmemset, iterate over the set to find first mapping and delete it. Turn on
 * part coalescing and map new part. Lastly iterate over the set to find the
 * coalesced mapping and delete it.
 */
static int
test_full_coalescing_after_remove_second_part_map(const struct test_case *tc,
		int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL(
			"usage: test_full_coalescing_after_remove_second_part_map" \
			"<path>");

	const char *file = argv[0];
	struct pmem2_source *pmem2_src;
	struct pmemset *set;
	struct pmemset_config *cfg;
	struct pmemset_part *part;
	struct pmemset_part_map *first_pmap = NULL;
	struct pmemset_part_map *second_pmap = NULL;
	struct pmemset_source *src;

	int fd = OPEN(file, O_RDWR);

	int ret = pmem2_source_from_fd(&pmem2_src, fd);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_source_from_pmem2(&src, pmem2_src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	create_config(&cfg);

	ret = pmemset_new(&set, cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	/* map first part */
	ret = pmemset_part_new(&part, set, src, 0, 0);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_part_map(&part, NULL, NULL);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	/* map second part */
	ret = pmemset_part_new(&part, set, src, 0, 0);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_part_map(&part, NULL, NULL);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	/* there should be two mapping in the pmemset */
	pmemset_first_part_map(set, &first_pmap);
	UT_ASSERTne(first_pmap, NULL);

	pmemset_next_part_map(set, first_pmap, &second_pmap);
	UT_ASSERTne(second_pmap, NULL);

	struct pmemset_part_descriptor desc_before;
	desc_before = pmemset_descriptor_part_map(first_pmap);

	/* delete second mapping */
	ret = pmemset_remove_part_map(set, &second_pmap);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(second_pmap, NULL);

	pmemset_part_map_drop(&first_pmap);

	pmemset_first_part_map(set, &first_pmap);
	UT_ASSERTne(first_pmap, NULL);

	pmemset_next_part_map(set, first_pmap, &second_pmap);
	UT_ASSERTeq(second_pmap, NULL);

	/* check if the first mapped part is still kept in pmemset */
	struct pmemset_part_descriptor desc_after;
	desc_after = pmemset_descriptor_part_map(first_pmap);
	UT_ASSERTeq(desc_before.addr, desc_after.addr);

	/* turn on coalescing */
	ret = pmemset_set_contiguous_part_coalescing(set,
			PMEMSET_COALESCING_FULL);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	/* new part should be coalesced with the remaining mapping */
	ret = pmemset_part_new(&part, set, src, 0, 0);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_part_map(&part, NULL, NULL);
	/* the address next to the previous part mapping is already occupied */
	if (ret == PMEMSET_E_CANNOT_COALESCE_PARTS) {
		pmemset_part_delete(&part);
		goto err_cleanup;
	}
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	pmemset_part_map_drop(&first_pmap);

	/* coalesced part mapping should be the only mapping in the pmemset */
	pmemset_first_part_map(set, &first_pmap);
	UT_ASSERTne(first_pmap, NULL);

	pmemset_next_part_map(set, first_pmap, &second_pmap);
	UT_ASSERTeq(second_pmap, NULL);

	ret = pmemset_remove_part_map(set, &first_pmap);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(first_pmap, NULL);

	/* after removing coalesced mapped part, pmemset should be empty */
	pmemset_first_part_map(set, &first_pmap);
	UT_ASSERTeq(first_pmap, NULL);

err_cleanup:
	ret = pmemset_delete(&set);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_config_delete(&cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_source_delete(&src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmem2_source_delete(&pmem2_src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	CLOSE(fd);

	return 1;
}

/*
 * test_remove_multiple_part_maps -- map hundred parts to the pmemset, iterate
 * over the set to find the middle part mapping and delete it, repeat the
 * process until there is not mappings left.
 */
static int
test_remove_multiple_part_maps(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_remove_multiple_part_maps <path>");

	const char *file = argv[0];
	struct pmem2_source *pmem2_src;
	struct pmemset *set;
	struct pmemset_config *cfg;
	struct pmemset_part *part;
	struct pmemset_part_map *pmap = NULL;
	struct pmemset_source *src;
	size_t part_size = 64 * 1024;
	size_t nmaps = 100;

	int fd = OPEN(file, O_RDWR);

	int ret = pmem2_source_from_fd(&pmem2_src, fd);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_source_from_pmem2(&src, pmem2_src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	create_config(&cfg);

	ret = pmemset_new(&set, cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	/* map hundred parts */
	for (int i = 0; i < nmaps; i++) {
		ret = pmemset_part_new(&part, set, src, 0, part_size);
		UT_PMEMSET_EXPECT_RETURN(ret, 0);

		ret = pmemset_part_map(&part, NULL, NULL);
		UT_PMEMSET_EXPECT_RETURN(ret, 0);
	}

	struct pmemset_part_map **pmaps =
			malloc(sizeof(*pmaps) * ((nmaps / 2) + 1));

	/* keep deleting until there's no mapping left in pmemset */
	while (nmaps) {
		pmemset_first_part_map(set, &pmaps[0]);
		UT_ASSERTne(pmaps[0], NULL);

		/* find middle part mapping */
		int i;
		for (i = 0; i < nmaps / 2; i++) {
			pmemset_next_part_map(set, pmaps[i], &pmaps[i + 1]);
			UT_ASSERTne(pmaps[i + 1], NULL);

			/* drop previous part map reference */
			pmemset_part_map_drop(&pmaps[i]);
		}

		ret = pmemset_remove_part_map(set, &pmaps[i]);
		UT_ASSERTeq(ret, 0);
		UT_ASSERTeq(pmap, NULL);

		nmaps--;
	}

	free(pmaps);

	pmemset_first_part_map(set, &pmap);
	UT_ASSERTeq(pmap, NULL);

	ret = pmemset_delete(&set);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_config_delete(&cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_source_delete(&src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmem2_source_delete(&pmem2_src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	CLOSE(fd);

	return 1;
}

/*
 * test_part_map_valid_source_temp - create a new part from a temp source
 *                                   with valid dir path and map part
 */
static int
test_part_map_valid_source_temp(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_part_map_valid_source_temp <dir>");

	const char *dir = argv[0];
	struct pmemset_part *part;
	struct pmemset_source *src;
	struct pmemset *set;
	struct pmemset_config *cfg;
	size_t part_size = 128 * 1024;
	struct pmemset_part_map *pmap = NULL;

	int ret = pmemset_source_from_temporary(&src, dir);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	create_config(&cfg);

	ret = pmemset_new(&set, cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_part_new(&part, set, src, 0, part_size);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	struct pmemset_part_descriptor desc;
	ret = pmemset_part_map(&part, NULL, &desc);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ASSERTeq(desc.size, part_size);
	UT_ASSERTeq(part, NULL);

	static size_t nmaps = 3;
	for (int i = 0; i < nmaps; i++) {
		ret = pmemset_part_new(&part, set, src, 0, part_size / 2);
		UT_PMEMSET_EXPECT_RETURN(ret, 0);

		ret = pmemset_part_map(&part, NULL, &desc);
		UT_PMEMSET_EXPECT_RETURN(ret, 0);
		ASSERTeq(desc.size, part_size / 2);
	}

	struct pmemset_part_map **pmaps =
			malloc(sizeof(*pmaps) * ((nmaps / 2) + 1));
	while (nmaps) {
		pmemset_first_part_map(set, &pmaps[0]);
		UT_ASSERTne(pmaps[0], NULL);

		/* find middle part mapping */
		int i;
		for (i = 0; i < nmaps / 2; i++) {
			pmemset_next_part_map(set, pmaps[i], &pmaps[i + 1]);
			UT_ASSERTne(pmaps[i + 1], NULL);

			/* drop previous part map reference */
			pmemset_part_map_drop(&pmaps[i]);
		}

		ret = pmemset_remove_part_map(set, &pmaps[i]);
		UT_ASSERTeq(ret, 0);
		UT_ASSERTeq(pmap, NULL);

		nmaps--;
	}

	free(pmaps);

	ret = pmemset_delete(&set);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_config_delete(&cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_source_delete(&src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	return 1;
}

/*
 * test_part_map_invalid_source_temp - create a new part from a temp source
 *                                    with invalid part size
 */
static int
test_part_map_invalid_source_temp(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_part_map_invalid_source_temp <dir>");

	const char *dir = argv[0];
	struct pmemset_part *part;
	struct pmemset_source *src;
	struct pmemset *set;
	struct pmemset_config *cfg;

	int ret = pmemset_source_from_temporary(&src, dir);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	create_config(&cfg);

	ret = pmemset_new(&set, cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_part_new(&part, set, src, 0, SIZE_MAX);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_part_map(&part, NULL, NULL);
	UT_PMEMSET_EXPECT_RETURN(ret, PMEMSET_E_CANNOT_TRUNCATE_SOURCE_FILE);

	ret = pmemset_delete(&set);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_config_delete(&cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_source_delete(&src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	return 1;
}

/*
 * test_part_map_source_truncate - create a new part from a file source
 *                                    with truncate flag
 */
static int
test_part_map_source_truncate(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_part_map_source_truncate <file>");

	const char *file = argv[0];
	struct pmemset_part *part;
	struct pmemset_source *src;
	struct pmemset *set;
	struct pmemset_config *cfg;
	size_t part_size = 64 * 1024;

	create_config(&cfg);

	int ret = pmemset_new(&set, cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	unsigned flags = PMEMSET_SOURCE_FILE_CREATE_IF_NEEDED;
	ret = pmemset_xsource_from_file(&src, file, flags);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(src, NULL);

	ret = os_access(file, F_OK);
	UT_ASSERTeq(ret, 0);

	os_stat_t st;
	ret = os_stat(file, &st);
	UT_ASSERTeq(ret, 0);

	os_off_t size = st.st_size;
	UT_ASSERT(size == 0);

	ret = pmemset_part_new(&part, set, src, 0, part_size);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	/* is should failed - size of file is too small  */
	ret = pmemset_part_map(&part, NULL, NULL);
	UT_PMEMSET_EXPECT_RETURN(ret, PMEMSET_E_INVALID_PMEM2_MAP);

	ret = pmemset_source_delete(&src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	/* try the same with truncate option */
	flags = PMEMSET_SOURCE_FILE_CREATE_IF_NEEDED | \
		PMEMSET_SOURCE_FILE_TRUNCATE_IF_NEEDED;
	ret = pmemset_xsource_from_file(&src, file, flags);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(src, NULL);

	ret = pmemset_part_new(&part, set, src, 0, part_size);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	struct pmemset_part_descriptor desc;
	ret = pmemset_part_map(&part, NULL, &desc);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ASSERTeq(desc.size, part_size);
	UT_ASSERTeq(part, NULL);

	ret = os_stat(file, &st);
	UT_ASSERTeq(ret, 0);

	size = st.st_size;
	UT_ASSERT(size == part_size);

	ret = pmemset_part_new(&part, set, src, 0, part_size * 2);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_part_map(&part, NULL, &desc);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ASSERTeq(desc.size, part_size * 2);

	ret = os_stat(file, &st);
	UT_ASSERTeq(ret, 0);

	size = st.st_size;
	UT_ASSERT(size == part_size * 2);

	ret = pmemset_delete(&set);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_config_delete(&cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_source_delete(&src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	return 1;
}

/*
 * test_remove_two_ranges -- map two files to the pmemset, iterate over the set
 * to get mappings' regions. Select a region that encrouches on both of those
 * mappings with minimum size and delete them.
 */
static int
test_remove_two_ranges(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_remove_two_ranges <path>");

	const char *file = argv[0];
	struct pmem2_source *pmem2_src;
	struct pmemset *set;
	struct pmemset_config *cfg;
	struct pmemset_part *part;
	struct pmemset_part_descriptor first_desc;
	struct pmemset_part_descriptor second_desc;
	struct pmemset_part_map *first_pmap;
	struct pmemset_part_map *second_pmap;
	struct pmemset_source *src;
	size_t part_size = 64 * 1024;

	int fd = OPEN(file, O_RDWR);

	int ret = pmem2_source_from_fd(&pmem2_src, fd);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_source_from_pmem2(&src, pmem2_src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	create_config(&cfg);

	ret = pmemset_new(&set, cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	int n_maps = 2;
	for (int i = 0; i < n_maps; i++) {
		ret = pmemset_part_new(&part, set, src, 0, part_size);
		UT_PMEMSET_EXPECT_RETURN(ret, 0);

		ret = pmemset_part_map(&part, NULL, NULL);
		UT_PMEMSET_EXPECT_RETURN(ret, 0);
	}

	pmemset_first_part_map(set, &first_pmap);
	UT_ASSERTne(first_pmap, NULL);
	first_desc = pmemset_descriptor_part_map(first_pmap);

	pmemset_next_part_map(set, first_pmap, &second_pmap);
	UT_ASSERTne(second_pmap, NULL);
	second_desc = pmemset_descriptor_part_map(second_pmap);

	pmemset_part_map_drop(&first_pmap);
	pmemset_part_map_drop(&second_pmap);

	char *first_end_addr = (char *)first_desc.addr + first_desc.size;
	size_t range_first_to_second = (size_t)second_desc.addr -
			(size_t)first_end_addr;
	ret = pmemset_remove_range(set, (char *)first_end_addr - 1,
			range_first_to_second + 2);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	pmemset_first_part_map(set, &first_pmap);
	UT_ASSERTeq(first_pmap, NULL);

	ret = pmemset_delete(&set);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_config_delete(&cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_source_delete(&src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmem2_source_delete(&pmem2_src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	CLOSE(fd);

	return 1;
}

/*
 * test_remove_coalesced_two_ranges -- create two coalesced mappings, each
 * composed of two parts, iterate over the set to get mappings' regions. Select
 * a region that encrouches on both of those coalesced mappings containing
 * one part each and delete them.
 */
static int
test_remove_coalesced_two_ranges(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_remove_coalesced_two_ranges <path>");

	const char *file = argv[0];
	struct pmem2_source *pmem2_src;
	struct pmemset *set;
	struct pmemset_config *cfg;
	struct pmemset_part *part;
	struct pmemset_part_descriptor first_desc;
	struct pmemset_part_descriptor second_desc;
	struct pmemset_part_map *first_pmap;
	struct pmemset_part_map *second_pmap;
	struct pmemset_source *src;
	size_t part_size = 64 * 1024;

	int fd = OPEN(file, O_RDWR);

	int ret = pmem2_source_from_fd(&pmem2_src, fd);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_source_from_pmem2(&src, pmem2_src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	create_config(&cfg);

	ret = pmemset_new(&set, cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	int n_maps = 4;
	for (int i = 0; i < n_maps; i++) {
		/* there will be two different coalesced part mappings */
		enum pmemset_coalescing coalescing;
		coalescing = (i == 1 || i == 3) ? PMEMSET_COALESCING_FULL :
				PMEMSET_COALESCING_NONE;
		pmemset_set_contiguous_part_coalescing(set, coalescing);

		ret = pmemset_part_new(&part, set, src, 0, part_size);
		UT_PMEMSET_EXPECT_RETURN(ret, 0);

		ret = pmemset_part_map(&part, NULL, NULL);
		if (ret == PMEMSET_E_CANNOT_COALESCE_PARTS) {
			pmemset_part_delete(&part);
			goto err_cleanup;
		}
		UT_PMEMSET_EXPECT_RETURN(ret, 0);
	}

	pmemset_first_part_map(set, &first_pmap);
	UT_ASSERTne(first_pmap, NULL);
	first_desc = pmemset_descriptor_part_map(first_pmap);

	pmemset_next_part_map(set, first_pmap, &second_pmap);
	UT_ASSERTne(second_pmap, NULL);
	second_desc = pmemset_descriptor_part_map(second_pmap);

	pmemset_part_map_drop(&first_pmap);
	pmemset_part_map_drop(&second_pmap);

	char *first_end_addr = (char *)first_desc.addr + first_desc.size;
	size_t range_first_to_second = (size_t)second_desc.addr -
			(size_t)first_end_addr;

	ret = pmemset_remove_range(set, (char *)first_end_addr - 1,
			range_first_to_second + 2);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	/* there should be two mappings left, each containg one part */
	pmemset_first_part_map(set, &first_pmap);
	UT_ASSERTne(first_pmap, NULL);
	first_desc = pmemset_descriptor_part_map(first_pmap);

	pmemset_next_part_map(set, first_pmap, &second_pmap);
	UT_ASSERTne(second_pmap, NULL);
	second_desc = pmemset_descriptor_part_map(second_pmap);

	struct pmem2_map *any_p2map;
	/* check if pmem2 mapping was deleted from first coalesced part map */
	UT_ASSERTeq(first_desc.size, part_size);
	pmem2_vm_reservation_map_find(first_pmap->pmem2_reserv, part_size,
			part_size, &any_p2map);
	UT_ASSERTeq(any_p2map, NULL);

	/* check if pmem2 mapping was deleted from second coalesced part map */
	UT_ASSERTeq(second_desc.size, part_size);
	/* vm reservation was resized, so the offset is also different */
	pmem2_vm_reservation_map_find(first_pmap->pmem2_reserv, part_size,
			part_size, &any_p2map);
	UT_ASSERTeq(any_p2map, NULL);

err_cleanup:
	ret = pmemset_delete(&set);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_config_delete(&cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_source_delete(&src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmem2_source_delete(&pmem2_src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	CLOSE(fd);

	return 1;
}

/*
 * test_remove_coalesced_middle_range -- create coalesced mapping composed of
 * three parts, iterate over the set to get mapping's region. Select a region
 * that encrouches only on the part situated in the middle of the coalesced
 * part mapping and delete it.
 */
static int
test_remove_coalesced_middle_range(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_remove_coalesced_middle_range <path>");

	const char *file = argv[0];
	struct pmem2_source *pmem2_src;
	struct pmemset *set;
	struct pmemset_config *cfg;
	struct pmemset_part *part;
	struct pmemset_part_descriptor desc;
	struct pmemset_part_map *pmap = NULL;
	struct pmemset_source *src;
	size_t part_size = 64 * 1024;

	int fd = OPEN(file, O_RDWR);

	int ret = pmem2_source_from_fd(&pmem2_src, fd);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_source_from_pmem2(&src, pmem2_src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	create_config(&cfg);

	ret = pmemset_new(&set, cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	pmemset_set_contiguous_part_coalescing(set, PMEMSET_COALESCING_FULL);

	int n_maps = 3;
	for (int i = 0; i < n_maps; i++) {
		ret = pmemset_part_new(&part, set, src, 0, part_size);
		UT_PMEMSET_EXPECT_RETURN(ret, 0);

		ret = pmemset_part_map(&part, NULL, NULL);
		if (ret == PMEMSET_E_CANNOT_COALESCE_PARTS) {
			pmemset_part_delete(&part);
			goto err_cleanup;
		}
		UT_PMEMSET_EXPECT_RETURN(ret, 0);
	}

	pmemset_first_part_map(set, &pmap);
	UT_ASSERTne(pmap, NULL);
	desc = pmemset_descriptor_part_map(pmap);
	struct pmem2_vm_reservation *p2rsv = pmap->pmem2_reserv;

	pmemset_part_map_drop(&pmap);

	ret = pmemset_remove_range(set, (char *)desc.addr + part_size, 1);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	struct pmem2_map *any_p2map;
	/* check if middle pmem2 mapping was deleted from coalesced part map */
	pmem2_vm_reservation_map_find(p2rsv, part_size, part_size,
			&any_p2map);
	UT_ASSERTeq(any_p2map, NULL);

err_cleanup:
	ret = pmemset_delete(&set);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_config_delete(&cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_source_delete(&src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmem2_source_delete(&pmem2_src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	CLOSE(fd);

	return 1;
}

#define MAX_THREADS 32

struct worker_args {
	size_t n_ops;
	struct pmemset *set;
	struct pmemset_source *src;
};

static void *
part_map_and_remove_worker(void *arg)
{
	struct worker_args *warg = arg;

	size_t n_ops = warg->n_ops;
	struct pmemset *set = warg->set;
	struct pmemset_source *src = warg->src;

	struct pmemset_part *part;
	size_t part_size = 64 * 1024;
	struct pmemset_part_descriptor desc;
	struct pmemset_part_map *pmap;

	int ret;
	for (size_t n = 0; n < n_ops; n++) {
		ret = pmemset_part_new(&part, set, src, 0, part_size);
		UT_PMEMSET_EXPECT_RETURN(ret, 0);

		ret = pmemset_part_map(&part, NULL, &desc);
		UT_PMEMSET_EXPECT_RETURN(ret, 0);

		ret = pmemset_part_map_by_address(set, &pmap, desc.addr);
		UT_PMEMSET_EXPECT_RETURN(ret, 0);

		ret = pmemset_remove_part_map(set, &pmap);
		UT_PMEMSET_EXPECT_RETURN(ret, 0);
		UT_ASSERTeq(pmap, NULL);
	}

	return NULL;
}

static void
run_worker(void *(worker_func)(void *arg), struct worker_args args[],
		size_t n_threads)
{
	os_thread_t threads[MAX_THREADS];

	for (size_t n = 0; n < n_threads; n++)
		THREAD_CREATE(&threads[n], NULL, worker_func, &args[n]);

	for (size_t n = 0; n < n_threads; n++)
		THREAD_JOIN(&threads[n], NULL);
}

/*
 * test_pmemset_async_map_remove_multiple_part_maps -- asynchronously map new
 * and remove multiple parts to the pmemset
 */
static int
test_pmemset_async_map_remove_multiple_part_maps(const struct test_case *tc,
		int argc, char *argv[])
{
	if (argc < 3)
	UT_FATAL("usage: test_vm_reserv_async_map_unmap_multiple_files "
				"<file> <threads> <ops/thread>");

	size_t n_threads = ATOU(argv[1]);
	if (n_threads > MAX_THREADS)
		UT_FATAL("threads %zu > MAX_THREADS %u",
				n_threads, MAX_THREADS);

	char *file = argv[0];
	size_t ops_per_thread = ATOU(argv[2]);
	struct pmem2_source *pmem2_src;
	struct pmemset *set;
	struct pmemset_config *cfg;
	struct pmemset_source *src;
	struct worker_args args[MAX_THREADS];

	int fd = OPEN(file, O_RDWR);

	int ret = pmem2_source_from_fd(&pmem2_src, fd);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_source_from_pmem2(&src, pmem2_src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	create_config(&cfg);

	ret = pmemset_new(&set, cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	for (size_t n = 0; n < n_threads; n++) {
		args[n].n_ops = ops_per_thread;
		args[n].set = set;
		args[n].src = src;
	}

	run_worker(part_map_and_remove_worker, args, n_threads);

	ret = pmemset_delete(&set);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_config_delete(&cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_source_delete(&src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmem2_source_delete(&pmem2_src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	CLOSE(fd);

	return 3;
}

/*
 * test_divide_coalesced_remove_obtained_pmaps -- create coalesced mapping
 * composed of five parts, remove pmemset range two times to divide initial
 * mapping into three mappings, remove all three mappings
 */
static int
test_divide_coalesced_remove_obtained_pmaps(const struct test_case *tc,
		int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_divide_coalesced_remove_obtained_pmaps " \
				"<path>");

	const char *file = argv[0];
	struct pmem2_source *pmem2_src;
	struct pmemset *set;
	struct pmemset_config *cfg;
	struct pmemset_part *part;
	struct pmemset_part_descriptor desc;
	struct pmemset_part_map *pmap = NULL;
	struct pmemset_source *src;
	size_t part_size = 64 * 1024;

	int fd = OPEN(file, O_RDWR);

	int ret = pmem2_source_from_fd(&pmem2_src, fd);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_source_from_pmem2(&src, pmem2_src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	create_config(&cfg);

	ret = pmemset_new(&set, cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	pmemset_set_contiguous_part_coalescing(set, PMEMSET_COALESCING_FULL);

	int n_maps = 5;
	for (int i = 0; i < n_maps; i++) {
		ret = pmemset_part_new(&part, set, src, 0, part_size);
		UT_PMEMSET_EXPECT_RETURN(ret, 0);

		ret = pmemset_part_map(&part, NULL, NULL);
		if (ret == PMEMSET_E_CANNOT_COALESCE_PARTS) {
			pmemset_part_delete(&part);
			goto err_cleanup;
		}
		UT_PMEMSET_EXPECT_RETURN(ret, 0);
	}

	pmemset_first_part_map(set, &pmap);
	UT_ASSERTne(pmap, NULL);
	desc = pmemset_descriptor_part_map(pmap);
	pmemset_part_map_drop(&pmap);

	ret = pmemset_remove_range(set, (char *)desc.addr + part_size,
			part_size);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_remove_range(set, (char *)desc.addr + 3 * part_size,
			part_size);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	for (int i = 0; i < 3; i++) {
		pmemset_first_part_map(set, &pmap);
		UT_ASSERTne(pmap, NULL);

		pmemset_remove_part_map(set, &pmap);
		UT_ASSERTeq(ret, 0);
		UT_ASSERTeq(pmap, NULL);
	}

	pmemset_first_part_map(set, &pmap);
	UT_ASSERTeq(pmap, NULL);

err_cleanup:
	ret = pmemset_delete(&set);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_config_delete(&cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_source_delete(&src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmem2_source_delete(&pmem2_src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	CLOSE(fd);

	return 1;
}

/*
 * test_part_map_with_set_reservation -- create a pmem2 vm reservation with the
 * size of three files and set it in the pmemset config, map three files to the
 * pmemset
 */
static int
test_part_map_with_set_reservation(const struct test_case *tc,
		int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_part_map_with_set_reservation " \
				"<path>");

	const char *file = argv[0];
	struct pmem2_vm_reservation *rsv;
	struct pmem2_source *pmem2_src;
	struct pmemset *set;
	struct pmemset_config *cfg;
	struct pmemset_part *part;
	struct pmemset_part_descriptor *descs;
	struct pmemset_part_map **pmaps;
	struct pmemset_source *src;
	size_t part_size = 64 * 1024;
	size_t rsv_size = 3 * part_size;

	int ret = pmem2_vm_reservation_new(&rsv, NULL, rsv_size);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTne(pmem2_vm_reservation_get_address(rsv), NULL);
	UT_ASSERTeq(pmem2_vm_reservation_get_size(rsv), rsv_size);

	int fd = OPEN(file, O_RDWR);

	ret = pmem2_source_from_fd(&pmem2_src, fd);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_source_from_pmem2(&src, pmem2_src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	create_config(&cfg);

	ret = pmemset_new(&set, cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	size_t n_maps = 3;
	for (int i = 0; i < n_maps; i++) {
		ret = pmemset_part_new(&part, set, src, 0, part_size);
		UT_PMEMSET_EXPECT_RETURN(ret, 0);

		ret = pmemset_part_map(&part, NULL, NULL);
		UT_PMEMSET_EXPECT_RETURN(ret, 0);
	}

	pmaps = malloc(sizeof(struct pmemset_part_map *) * n_maps);
	descs = malloc(sizeof(struct pmemset_part_descriptor) * n_maps);

	pmemset_first_part_map(set, &pmaps[0]);
	UT_ASSERTne(pmaps[0], NULL);
	descs[0] = pmemset_descriptor_part_map(pmaps[0]);

	for (int i = 1; i < n_maps; i++) {
		pmemset_next_part_map(set, pmaps[i - 1], &pmaps[i]);
		UT_ASSERTne(pmaps[i], NULL);

		descs[i] = pmemset_descriptor_part_map(pmaps[i]);
		UT_ASSERTne(descs[i - 1].addr, descs[i].addr);
	}

	for (int i = 0; i < n_maps; i++) {
		ret = pmemset_remove_part_map(set, &pmaps[i]);
		UT_PMEMSET_EXPECT_RETURN(ret, 0);
		UT_ASSERTeq(pmaps[i], NULL);
	}

	UT_ASSERTeq(pmem2_vm_reservation_get_size(rsv), rsv_size);

	pmemset_first_part_map(set, &pmaps[0]);
	UT_ASSERTeq(pmaps[0], NULL);

	free(pmaps);
	free(descs);
	ret = pmemset_delete(&set);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_config_delete(&cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_source_delete(&src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmem2_source_delete(&pmem2_src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	CLOSE(fd);

	return 1;
}

/*
 * test_part_map_coalesce_with_set_reservation -- create a pmem2 vm reservation
 * with the size of three files and set it in the pmemset config, turn on full
 * coalescing and map three files to the pmemset
 */
static int
test_part_map_coalesce_with_set_reservation(const struct test_case *tc,
		int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_part_map_coalesce_with_set_reservation " \
				"<path>");

	const char *file = argv[0];
	struct pmem2_vm_reservation *rsv;
	struct pmem2_source *pmem2_src;
	struct pmemset *set;
	struct pmemset_config *cfg;
	struct pmemset_part *part;
	struct pmemset_part_map *pmap;
	struct pmemset_source *src;
	size_t part_size = 64 * 1024;
	size_t rsv_size = 3 * part_size;

	int ret = pmem2_vm_reservation_new(&rsv, NULL, rsv_size);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTne(pmem2_vm_reservation_get_address(rsv), NULL);
	UT_ASSERTeq(pmem2_vm_reservation_get_size(rsv), rsv_size);

	int fd = OPEN(file, O_RDWR);

	ret = pmem2_source_from_fd(&pmem2_src, fd);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_source_from_pmem2(&src, pmem2_src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	create_config(&cfg);

	ret = pmemset_new(&set, cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	pmemset_set_contiguous_part_coalescing(set, PMEMSET_COALESCING_FULL);

	size_t n_maps = 3;
	for (int i = 0; i < n_maps; i++) {
		ret = pmemset_part_new(&part, set, src, 0, part_size);
		UT_PMEMSET_EXPECT_RETURN(ret, 0);

		ret = pmemset_part_map(&part, NULL, NULL);
		if (ret == PMEMSET_E_CANNOT_COALESCE_PARTS) {
			pmemset_part_delete(&part);
			goto err_cleanup;
		}
		UT_PMEMSET_EXPECT_RETURN(ret, 0);
	}

	pmemset_first_part_map(set, &pmap);
	UT_ASSERTne(pmap, NULL);

	pmemset_remove_part_map(set, &pmap);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(pmap, NULL);

	pmemset_first_part_map(set, &pmap);
	UT_ASSERTeq(pmap, NULL);

	UT_ASSERTeq(pmem2_vm_reservation_get_size(rsv), rsv_size);

	pmemset_first_part_map(set, &pmap);
	UT_ASSERTeq(pmap, NULL);

err_cleanup:
	ret = pmemset_delete(&set);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_config_delete(&cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_source_delete(&src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmem2_source_delete(&pmem2_src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	CLOSE(fd);

	return 1;
}

/*
 * test_cases -- available test cases
 */
static struct test_case test_cases[] = {
	TEST_CASE(test_part_new_enomem),
	TEST_CASE(test_part_new_invalid_source_file),
	TEST_CASE(test_part_new_valid_source_file),
	TEST_CASE(test_part_new_valid_source_pmem2),
	TEST_CASE(test_part_map_valid_source_pmem2),
	TEST_CASE(test_part_map_valid_source_file),
	TEST_CASE(test_part_map_invalid_offset),
	TEST_CASE(test_part_map_gran_read),
	TEST_CASE(test_unmap_part),
	TEST_CASE(test_part_map_enomem),
	TEST_CASE(test_part_map_first),
	TEST_CASE(test_part_map_descriptor),
	TEST_CASE(test_part_map_next),
	TEST_CASE(test_part_map_drop),
	TEST_CASE(test_part_map_by_addr),
	TEST_CASE(test_part_map_unaligned_size),
	TEST_CASE(test_part_map_full_coalesce_before),
	TEST_CASE(test_part_map_full_coalesce_after),
	TEST_CASE(test_part_map_opp_coalesce_before),
	TEST_CASE(test_part_map_opp_coalesce_after),
	TEST_CASE(test_remove_part_map),
	TEST_CASE(test_full_coalescing_before_remove_part_map),
	TEST_CASE(test_full_coalescing_after_remove_first_part_map),
	TEST_CASE(test_full_coalescing_after_remove_second_part_map),
	TEST_CASE(test_remove_multiple_part_maps),
	TEST_CASE(test_part_map_valid_source_temp),
	TEST_CASE(test_part_map_invalid_source_temp),
	TEST_CASE(test_part_map_source_truncate),
	TEST_CASE(test_remove_two_ranges),
	TEST_CASE(test_remove_coalesced_two_ranges),
	TEST_CASE(test_remove_coalesced_middle_range),
	TEST_CASE(test_pmemset_async_map_remove_multiple_part_maps),
	TEST_CASE(test_divide_coalesced_remove_obtained_pmaps),
	TEST_CASE(test_part_map_with_set_reservation),
	TEST_CASE(test_part_map_coalesce_with_set_reservation),
};

#define NTESTS (sizeof(test_cases) / sizeof(test_cases[0]))

int
main(int argc, char **argv)
{
	START(argc, argv, "pmemset_part");

	util_init();
	out_init("pmemset_part", "TEST_LOG_LEVEL", "TEST_LOG_FILE", 0, 0);
	TEST_CASE_PROCESS(argc, argv, test_cases, NTESTS);
	out_fini();

	DONE(NULL);
}

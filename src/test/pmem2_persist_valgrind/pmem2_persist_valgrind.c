/*
 * Copyright 2020, Intel Corporation
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
 * pmem2_persist_valgrind.c -- pmem2_persist_valgrind tests
 */

#include "unittest.h"
#include "ut_pmem2_utils.h"

#define DATA "XXXXXXXX"

static size_t LENGTH = sizeof(DATA);

/*
 * test_ctx -- essential parameters used by test
 */
struct test_ctx {
	int fd;
	enum pmem2_granularity required_granularity;
};

/*
 * init_test -- initialize basic parameters for test
 */
static void
init_test(int argc, char *argv[], struct test_ctx *ctx)
{
	if (argc != 2)
		UT_FATAL("usage: test_persist_fn <file>");

	char *file = argv[0];
	ctx->fd = OPEN(file, O_RDWR);

	ctx->required_granularity = (enum pmem2_granularity)atoi(argv[1]);
}
/*
 * prepare_config -- fill pmem2_config in minimal scope
 */
static void
prepare_config(struct pmem2_config **cfg, struct test_ctx *ctx)
{
	int ret = pmem2_config_new(cfg);
	UT_PMEM2_EXPECT_RETURN(ret, 0);

	if (ctx->fd != -1) {
		ret = pmem2_config_set_fd(*cfg, ctx->fd);
		UT_PMEM2_EXPECT_RETURN(ret, 0);
	}

	ret = pmem2_config_set_required_store_granularity(
		*cfg, ctx->required_granularity);
	UT_PMEM2_EXPECT_RETURN(ret, 0);
}

/*
 * map_valid -- return valid mapped pmem2_map and validate mapped memory length
 */
static struct pmem2_map *
map_valid(struct pmem2_config *cfg, size_t size)
{
	struct pmem2_map *map = NULL;
	int ret = pmem2_map(cfg, &map);
	UT_PMEM2_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(map, NULL);
	UT_ASSERTeq(pmem2_map_get_size(map), size);

	return map;
}

/*
 * write_data -- write some data in mapped memory
 */
static void
write_data(struct pmem2_map *map, size_t interval)
{
	size_t i = 0;
	char *addr = pmem2_map_get_address(map);
	size_t map_size = pmem2_map_get_size(map);

	while (i < (map_size - LENGTH)) {
		memcpy(addr + i, DATA, LENGTH);
		i += interval;
	}
}

/*
 * persist_continuous_data -- persist continuous data in range of mapped memory
 */
static void
persist_continuous_data(struct pmem2_map *map)
{
	size_t map_size = pmem2_map_get_size(map);
	char *addr = pmem2_map_get_address(map);
	pmem2_persist_fn p_func = pmem2_get_persist_fn(map);
	p_func(addr, map_size);
}

/*
 * persist_discontinuous_data -- persist discontinuous data in range of mapped
 * memory in places where the data is written
 */
static void
persist_discontinuous_data(struct pmem2_map *map)
{
	int i = 0;
	size_t map_size = pmem2_map_get_size(map);
	char *addr = pmem2_map_get_address(map);
	pmem2_persist_fn p_func = pmem2_get_persist_fn(map);
	while (i <= (map_size - LENGTH)) {
		p_func(addr + i, LENGTH);
		i += EXEC_PAGESIZE * 2;
	};
}

typedef void (*persist_data_fn)(struct pmem2_map *map);

/*
 * persist_test_template -- template for testing pmem2_persist_fn
 */
static void
persist_test_template(int argc, char *argv[], persist_data_fn persist_fn,
			size_t interval)
{
	struct test_ctx ctx = {0};
	init_test(argc, argv, &ctx);

	struct pmem2_config *cfg;
	prepare_config(&cfg, &ctx);

	size_t size;
	UT_ASSERTeq(pmem2_config_get_file_size(cfg, &size), 0);

	struct pmem2_map *map = map_valid(cfg, size);

	write_data(map, interval);

	persist_fn(map);

	/* cleanup after the test */
	pmem2_unmap(&map);
	pmem2_config_delete(&cfg);
	CLOSE(ctx.fd);
}

/*
 * test_persist_continuous_range -- persist continuous data in a range of
 * the persistent memory
 */
static int
test_persist_continuous_range(const struct test_case *tc, int argc,
				char *argv[])
{
	persist_test_template(argc, argv, persist_continuous_data, LENGTH);

	return 2;
}

/*
 * test_persist_discontinuous_range -- persist discontinuous data in a range of
 * the persistent memory
 */
static int
test_persist_discontinuous_range(const struct test_case *tc, int argc,
					char *argv[])
{
	persist_test_template(argc, argv, persist_discontinuous_data,
				EXEC_PAGESIZE * 2);

	return 2;
}

/*
 * test_persist_discontinuous_range_partially -- persist part of discontinuous
 * data in a range of the persistent memory
 */
static int
test_persist_discontinuous_range_partially(const struct test_case *tc, int argc,
						char *argv[])
{
	persist_test_template(argc, argv, persist_discontinuous_data,
				EXEC_PAGESIZE);

	return 2;
}

/*
 * test_persist_nonpmem_data -- persist data in a range of the memory mapped
 * by mmap
 */
static int
test_persist_nonpmem_data(const struct test_case *tc, int argc, char *argv[])
{
	struct test_ctx ctx = {0};
	init_test(argc, argv, &ctx);

	struct pmem2_config *cfg;
	prepare_config(&cfg, &ctx);

	size_t size;
	UT_ASSERTeq(pmem2_config_get_file_size(cfg, &size), 0);

	int flags = MAP_SHARED;
	int proto = PROT_READ | PROT_WRITE;

	void *addr;
	addr = mmap(NULL, size, proto, flags, ctx.fd, 0);

	munmap(addr, 0);
	pmem2_config_delete(&cfg);
	CLOSE(ctx.fd);

	return 2;
}

/*
 * test_cases -- available test cases
 */
static struct test_case test_cases[] = {
	TEST_CASE(test_persist_continuous_range),
	TEST_CASE(test_persist_discontinuous_range),
	TEST_CASE(test_persist_discontinuous_range_partially),
	TEST_CASE(test_persist_nonpmem_data),
};

#define NTESTS (sizeof(test_cases) / sizeof(test_cases[0]))

int
main(int argc, char *argv[])
{
	START(argc, argv, "pmem2_persist_valgrind");
	TEST_CASE_PROCESS(argc, argv, test_cases, NTESTS);
	DONE(NULL);
}

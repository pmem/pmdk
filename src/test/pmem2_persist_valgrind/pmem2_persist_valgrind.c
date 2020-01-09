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

#include "out.h"
#include "unittest.h"
#include "ut_pmem2_utils.h"

#define DATA "XXXXXXXX"
#define STRIDE_SIZE 4096

/*
 * test_ctx -- essential parameters used by test
 */
struct test_ctx {
	int fd;
	struct pmem2_map *map;
};

/*
 * test_init -- prepare resources required for testing
 */
static int
test_init(const struct test_case *tc, int argc, char *argv[],
		struct test_ctx *ctx)
{
	if (argc < 1)
		UT_FATAL("usage: %s <file>", tc->name);

	char *file = argv[0];
	ctx->fd = OPEN(file, O_RDWR);

	struct pmem2_config *cfg;
	/* fill pmem2_config in minimal scope */
	int ret = pmem2_config_new(&cfg);
	UT_PMEM2_EXPECT_RETURN(ret, 0);

	ret = pmem2_config_set_fd(cfg, ctx->fd);
	UT_PMEM2_EXPECT_RETURN(ret, 0);

	ret = pmem2_config_set_required_store_granularity(
		cfg, PMEM2_GRANULARITY_PAGE);
	UT_PMEM2_EXPECT_RETURN(ret, 0);

	/* execute pmem2_map and validate the result */
	ret = pmem2_map(cfg, &ctx->map);
	UT_PMEM2_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(ctx->map, NULL);

	size_t size;
	UT_ASSERTeq(pmem2_config_get_file_size(cfg, &size), 0);
	UT_ASSERTeq(pmem2_map_get_size(ctx->map), size);

	pmem2_config_delete(&cfg);

	/* the function returns the number of consumed arguments */
	return 1;
}

/*
 * test_fini -- cleanup the test resources
 */
static void
test_fini(struct test_ctx *ctx)
{
	pmem2_unmap(&ctx->map);
	CLOSE(ctx->fd);
}

/*
 * data_write -- write the data in mapped memory
 */
static void
data_write(void *addr, size_t size, size_t stride)
{
	for (size_t offset = 0; offset + sizeof(DATA) <= size;
		offset += stride) {
		memcpy((void *)((uintptr_t)addr + offset), DATA, sizeof(DATA));
	}
}

/*
 * data_persist -- persist data in a range of mapped memory with defined stride
 */
static void
data_persist(struct pmem2_map *map, size_t len, size_t stride)
{
	size_t map_size = pmem2_map_get_size(map);
	char *addr = pmem2_map_get_address(map);
	pmem2_persist_fn p_func = pmem2_get_persist_fn(map);

	for (size_t offset = 0; offset + len <= map_size;
		offset += stride) {
		p_func(addr + offset, len);
	}
}

/*
 * test_persist_continuous_range -- persist continuous data in a range of
 * the persistent memory
 */
static int
test_persist_continuous_range(const struct test_case *tc, int argc,
				char *argv[])
{
	struct test_ctx ctx = {0};
	int ret = test_init(tc, argc, argv, &ctx);

	char *addr = pmem2_map_get_address(ctx.map);
	size_t map_size = pmem2_map_get_size(ctx.map);
	data_write(addr, map_size, sizeof(DATA) /* stride */);
	data_persist(ctx.map, map_size, map_size /* stride */);

	test_fini(&ctx);

	return ret;
}

/*
 * test_persist_discontinuous_range -- persist discontinuous data in a range of
 * the persistent memory
 */
static int
test_persist_discontinuous_range(const struct test_case *tc, int argc,
					char *argv[])
{
	struct test_ctx ctx = {0};
	int ret = test_init(tc, argc, argv, &ctx);

	char *addr = pmem2_map_get_address(ctx.map);
	size_t map_size = pmem2_map_get_size(ctx.map);
	data_write(addr, map_size, STRIDE_SIZE);
	data_persist(ctx.map, sizeof(DATA), STRIDE_SIZE);

	test_fini(&ctx);

	return ret;
}

/*
 * test_persist_discontinuous_range_partially -- persist part of discontinuous
 * data in a range of persistent memory
 */
static int
test_persist_discontinuous_range_partially(const struct test_case *tc, int argc,
						char *argv[])
{
	struct test_ctx ctx = {0};
	int ret = test_init(tc, argc, argv, &ctx);

	char *addr = pmem2_map_get_address(ctx.map);
	size_t map_size = pmem2_map_get_size(ctx.map);
	data_write(addr, map_size, STRIDE_SIZE);
	/* persist only a half of the writes */
	data_persist(ctx.map, sizeof(DATA), 2 * STRIDE_SIZE);

	test_fini(&ctx);

	return ret;
}

/*
 * test_persist_nonpmem_data -- persist data in a range of the memory mapped
 * by mmap()
 */
static int
test_persist_nonpmem_data(const struct test_case *tc, int argc, char *argv[])
{
	struct test_ctx ctx = {0};
	/* pmem2_map is needed to get persist function */
	int ret = test_init(tc, argc, argv, &ctx);

	size_t size = pmem2_map_get_size(ctx.map);

	int flags = MAP_SHARED;
	int proto = PROT_READ | PROT_WRITE;

	char *addr;
	addr = mmap(NULL, size, proto, flags, ctx.fd, 0);
	data_write(addr, size, sizeof(DATA) /* stride */);

	pmem2_persist_fn p_func = pmem2_get_persist_fn(ctx.map);
	p_func(addr, size);

	munmap(addr, size);
	test_fini(&ctx);

	return ret;
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
	out_init("pmem2_persist_valgrind", "TEST_LOG_LEVEL", "TEST_LOG_FILE", 0,
			0);
	TEST_CASE_PROCESS(argc, argv, test_cases, NTESTS);
	out_fini();
	DONE(NULL);
}

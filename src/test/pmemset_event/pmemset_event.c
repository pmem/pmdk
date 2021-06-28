// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020-2021, Intel Corporation */

/*
 * pmemset_events.c -- pmemset_events unittests
 */

#include <string.h>
#ifndef _WIN32
#include <pthread.h>
#endif

#include "fault_injection.h"
#include "unittest.h"
#include "ut_pmemset_utils.h"

/*
 * create_config - create and initialize config
 */
static void create_config(struct pmemset_config **cfg)
{
	int ret = pmemset_config_new(cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(cfg, NULL);

	ret = pmemset_config_set_required_store_granularity(*cfg,
		PMEM2_GRANULARITY_PAGE);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(cfg, NULL);
}

/*
 * map -- map pmemset_source
 */
static void map(struct pmemset *set, struct pmemset_source *src,
	struct pmemset_part_descriptor *desc)
{
	struct pmemset_part *part;
	int ret = pmemset_part_new(&part, set, src, 0, 0);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_part_map(&part, NULL, desc);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
}

/*
 * cleanup -- perform cleanup after the test
 */
static void cleanup(struct pmemset *set, struct pmemset_config *cfg,
	struct pmemset_source *src, struct pmem2_source *pmem2_src, int fd)
{
	int ret = pmemset_delete(&set);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_config_delete(&cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_source_delete(&src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmem2_source_delete(&pmem2_src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	CLOSE(fd);
}

struct persist_args {
	void *addr;
	size_t len;
	int drains;
};

/*
 * persist_callback -- callback used in pmemset_persist_event test
 */
static int
persist_callback(struct pmemset *set, struct pmemset_event_context *ctx,
	void *arg)
{

	if (ctx->type == PMEMSET_EVENT_FLUSH) {
		struct persist_args *persist = arg;

		persist->addr = ctx->data.flush.addr;
		persist->len = ctx->data.flush.len;
	}
	if (ctx->type == PMEMSET_EVENT_DRAIN) {
		struct persist_args *persist = arg;
		persist->drains++;
	}

	return 0;
}

#define MASK_ADDR (void *)0xFFBADFF
#define MASK_LEN 0xFFFF
/*
 * test_pmemset_persist_event -- test PMEMSET_EVENT_FLUSH and
 * PMEMSET_EVENT_DRAIN events
 */
static int
test_pmemset_persist_event(const struct test_case *tc,
		int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_pmemset_flush_event <file>");

	char *file = argv[0];

	struct pmem2_source *pmem2_src;
	struct pmemset *set;
	struct pmemset_config *cfg;
	struct pmemset_source *src;
	struct pmemset_part_descriptor desc;
	struct persist_args args;
	args.drains = 0;

	int fd = OPEN(file, O_RDWR);

	int ret = pmem2_source_from_fd(&pmem2_src, fd);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_source_from_pmem2(&src, pmem2_src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	create_config(&cfg);
	pmemset_config_set_event_callback(cfg, &persist_callback, &args);
	ret = pmemset_new(&set, cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	map(set, src, &desc);

	pmemset_flush(set, desc.addr, desc.size);
	UT_ASSERTeq(desc.addr, args.addr);
	UT_ASSERTeq(desc.size, args.len);
	UT_ASSERTeq(args.drains, 0);

	args.addr = MASK_ADDR;
	args.len = MASK_LEN;

	pmemset_drain(set);
	UT_ASSERTeq(args.addr, MASK_ADDR);
	UT_ASSERTeq(args.len, MASK_LEN);
	UT_ASSERTeq(args.drains, 1);

	void *new_addr = (char *)desc.addr + 100;
	size_t new_size = desc.size - 100;

	pmemset_persist(set, new_addr, new_size);
	UT_ASSERTeq(args.addr, new_addr);
	UT_ASSERTeq(args.len, new_size);
	UT_ASSERTeq(args.drains, 2);

	cleanup(set, cfg, src, pmem2_src, fd);

	return 1;
}

struct copy_args {
	int count;
	int drains;
};

/*
 * copy_callback -- callback used in pmemset_copy_event test
 */
static int
copy_callback(struct pmemset *set, struct pmemset_event_context *ctx, void *arg)
{
	struct copy_args *copy = arg;

	if (ctx->type == PMEMSET_EVENT_FLUSH)
		UT_FATAL(
			"pmemset_copy|pmemset_mov|pmemset_set should not fire flush event");

	if (ctx->type == PMEMSET_EVENT_DRAIN) {
		copy->drains++;
		return 0;
	}

	char *dest;
	char *src;
	size_t len;
	if (ctx->type == PMEMSET_EVENT_SET) {
		copy->count++;
		char *addr = ctx->data.set.dest;
		char value = (char)ctx->data.set.value;
		if (addr[0] != value)
			return 0;

		dest = addr;
		src = addr + 1;
		len = ctx->data.set.len - 1;
	} else if (ctx->type == PMEMSET_EVENT_COPY) {
		copy->count++;
		dest = ctx->data.copy.dest;
		src = ctx->data.copy.src;
		len = ctx->data.copy.len;
	} else if (ctx->type == PMEMSET_EVENT_MOVE) {
		copy->count++;
		dest = ctx->data.move.dest;
		src = ctx->data.move.src;
		len = ctx->data.move.len;
	} else {
		return 0;
	}

	if (memcmp(dest, src, len) == 0)
		goto fatal;

	return 0;
fatal:
	UT_FATAL(
		"PMEMSET_EVENT_COPY|MOVE|SET should be fired before the operation");

}

#define DATA_SIZE 10000

/*
 * test_pmemset_copy_event -- test PMEMSET_EVENT_COPY PMEMSET_EVENT_MOVE and
 * PMEMSET_EVENT_SET events
 */
static int
test_pmemset_copy_event(const struct test_case *tc,
		int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_pmemset_flush_event <file>");

	char *file = argv[0];

	struct pmem2_source *pmem2_src;
	struct pmemset *set;
	struct pmemset_config *cfg;
	struct pmemset_source *src;
	struct pmemset_part_descriptor desc;
	struct copy_args args;
	args.drains = 0;
	args.count = 0;

	int fd = OPEN(file, O_RDWR);

	int ret = pmem2_source_from_fd(&pmem2_src, fd);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_source_from_pmem2(&src, pmem2_src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	create_config(&cfg);
	pmemset_config_set_event_callback(cfg, &copy_callback, &args);
	ret = pmemset_new(&set, cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	map(set, src, &desc);

	char *addr = desc.addr;
	pmemset_memset(set, addr, 0xBB, DATA_SIZE, 0);
	UT_ASSERTeq(args.drains, 1);
	UT_ASSERTeq(args.count, 1);
	pmemset_memcpy(set, addr + DATA_SIZE, addr, DATA_SIZE, 0);
	UT_ASSERTeq(args.drains, 2);
	UT_ASSERTeq(args.count, 2);
	pmemset_memmove(set, addr + DATA_SIZE * 2, addr + DATA_SIZE,
		DATA_SIZE, 0);
	UT_ASSERTeq(args.drains, 3);
	UT_ASSERTeq(args.count, 3);

	pmemset_memset(set, addr, 0xFF, DATA_SIZE, PMEMSET_F_MEM_NODRAIN);
	UT_ASSERTeq(args.drains, 3);
	UT_ASSERTeq(args.count, 4);
	pmemset_memcpy(set, addr + DATA_SIZE, addr, DATA_SIZE,
		PMEMSET_F_MEM_NODRAIN);
	UT_ASSERTeq(args.drains, 3);
	UT_ASSERTeq(args.count, 5);
	pmemset_memmove(set, addr + DATA_SIZE * 2, addr + DATA_SIZE,
		DATA_SIZE, PMEMSET_F_MEM_NODRAIN);
	UT_ASSERTeq(args.drains, 3);
	UT_ASSERTeq(args.count, 6);

	pmemset_drain(set);

	cleanup(set, cfg, src, pmem2_src, fd);

	return 1;
}

/*
 * test_cases -- available test cases
 */
static struct test_case test_cases[] = {
	TEST_CASE(test_pmemset_persist_event),
	TEST_CASE(test_pmemset_copy_event),
};

#define NTESTS (sizeof(test_cases) / sizeof(test_cases[0]))

int
main(int argc, char **argv)
{
	START(argc, argv, "pmemset_event");

	TEST_CASE_PROCESS(argc, argv, test_cases, NTESTS);

	DONE(NULL);
}

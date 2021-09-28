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
#include "source.h"
#include "unittest.h"
#include "ut_pmemset_utils.h"

/*
 * map -- map pmemset_source
 */
static void map(struct pmemset *set, struct pmemset_source *src,
	struct pmemset_part_descriptor *desc)
{
	int ret = pmemset_map(set, src, NULL, desc);
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

	ut_create_set_config(&cfg);
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

	ut_create_set_config(&cfg);
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

static struct pmemset *set_ptr;

struct part_add_remove_args {
	void *addr;
	size_t size;
	int count;
};

static int
part_add_callback(struct pmemset *set, struct pmemset_event_context *ctx,
		void *arg)
{
	UT_ASSERTeq(set, set_ptr);

	struct part_add_remove_args *cb_arg = arg;
	if (ctx->type == PMEMSET_EVENT_PART_ADD) {
		cb_arg->addr = ctx->data.part_add.addr;
		cb_arg->size = ctx->data.part_add.len;
		cb_arg->count++;
	}

	return 0;
}

/*
 * test_pmemset_part_add_event - set a part add event callback in a pmemset and
 *                               map a part to this set
 */
static int
test_pmemset_part_add_event(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_pmemset_part_add_event <path>");

	const char *file = argv[0];
	struct pmem2_source *pmem2_src;
	struct pmemset_source *src;
	struct pmemset *set;
	struct pmemset_config *cfg;
	struct pmemset_part_descriptor desc;
	struct part_add_remove_args args = { .addr = NULL, .size = 0,
				.count = 0 };

	int fd = OPEN(file, O_RDWR);

	int ret = pmem2_source_from_fd(&pmem2_src, fd);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_source_from_pmem2(&src, pmem2_src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ut_create_set_config(&cfg);
	pmemset_config_set_event_callback(cfg, part_add_callback, &args);

	ret = pmemset_new(&set, cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	set_ptr = set;

	map(set, src, &desc);
	UT_ASSERTeq(args.addr, desc.addr);
	UT_ASSERTeq(args.size, desc.size);
	UT_ASSERTeq(args.count, 1);

	map(set, src, &desc);
	UT_ASSERTeq(args.addr, desc.addr);
	UT_ASSERTeq(args.size, desc.size);
	UT_ASSERTeq(args.count, 2);

	cleanup(set, cfg, src, pmem2_src, fd);

	return 1;
}

static int
part_remove_callback(struct pmemset *set, struct pmemset_event_context *ctx,
		void *arg)
{
	UT_ASSERTeq(set, set_ptr);

	struct part_add_remove_args *cb_arg = arg;
	if (ctx->type == PMEMSET_EVENT_PART_REMOVE) {
		cb_arg->addr = ctx->data.part_remove.addr;
		cb_arg->size = ctx->data.part_remove.len;
		cb_arg->count++;
	}

	return 0;
}

/*
 * test_pmemset_part_remove_event - set a part remove event callback in the
 * pmemset config, map 2 parts to it, then remove those parts one by one.
 */
static int
test_pmemset_part_remove_event(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_pmemset_part_remove_event <path>");

	const char *file = argv[0];
	struct pmem2_source *pmem2_src;
	struct pmemset *set;
	struct pmemset_part_map *pmap;
	struct pmemset_part_descriptor desc;
	struct pmemset_source *src;
	struct pmemset_config *cfg;
	struct part_add_remove_args args =
			{ .addr = NULL, .size = 0, .count = 0 };

	int fd = OPEN(file, O_RDWR);

	int ret = pmem2_source_from_fd(&pmem2_src, fd);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_source_from_pmem2(&src, pmem2_src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ut_create_set_config(&cfg);

	pmemset_config_set_event_callback(cfg, part_remove_callback, &args);

	ret = pmemset_new(&set, cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	set_ptr = set;

	map(set, src, &desc);

	pmemset_first_part_map(set, &pmap);
	UT_ASSERTne(pmap, NULL);

	ret = pmemset_remove_part_map(set, &pmap);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(args.addr, desc.addr);
	UT_ASSERTeq(args.size, desc.size);
	UT_ASSERTeq(args.count, 1);

	cleanup(set, cfg, src, pmem2_src, fd);

	return 1;
}

struct remove_range_args {
	void *range_addr;
	size_t range_size;
	size_t remove_range_count;
	size_t part_remove_count;
};

static int
remove_range_callback(struct pmemset *set, struct pmemset_event_context *ctx,
		void *arg)
{
	UT_ASSERTeq(set, set_ptr);

	struct remove_range_args *cb_arg = arg;
	if (ctx->type == PMEMSET_EVENT_REMOVE_RANGE) {
		cb_arg->range_addr = ctx->data.remove_range.addr;
		cb_arg->range_size = ctx->data.remove_range.len;
		cb_arg->remove_range_count++;
	} else if (ctx->type == PMEMSET_EVENT_PART_REMOVE) {
		cb_arg->part_remove_count++;
	}

	return 0;
}

/*
 * test_pmemset_remove_range_event - set a part remove event callback in the
 * pmemset config, map 2 parts to it, then remove the range encompassing both of
 * them
 */
static int
test_pmemset_remove_range_event(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_pmemset_remove_range_event <path>");

	const char *file = argv[0];
	struct pmemset *set;
	struct pmemset_part_descriptor first_desc;
	struct pmemset_part_descriptor second_desc;
	struct pmemset_source *src;
	struct pmemset_config *cfg;
	struct remove_range_args args = { .range_addr = NULL, .range_size = 0,
			.remove_range_count = 0, .part_remove_count = 0 };

	int ret = pmemset_source_from_file(&src, file);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ut_create_set_config(&cfg);

	pmemset_config_set_event_callback(cfg, remove_range_callback, &args);

	ret = pmemset_new(&set, cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	set_ptr = set;

	map(set, src, &first_desc);
	map(set, src, &second_desc);

	void *encompassing_addr = (first_desc.addr < second_desc.addr) ?
			first_desc.addr : second_desc.addr;
	size_t encompassing_end_addr = (first_desc.addr < second_desc.addr) ?
			(size_t)second_desc.addr + second_desc.size :
			(size_t)first_desc.addr + first_desc.size;
	size_t encompassing_size = encompassing_end_addr -
			(size_t)encompassing_addr;

	ret = pmemset_remove_range(set, encompassing_addr, encompassing_size);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(args.range_addr, encompassing_addr);
	UT_ASSERTeq(args.range_size, encompassing_size);
	UT_ASSERTeq(args.remove_range_count, 1);
	UT_ASSERTeq(args.part_remove_count, 2);

	ret = pmemset_delete(&set);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_config_delete(&cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_source_delete(&src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	return 1;
}

struct sds_update_args {
	size_t count;
};

/*
 * sds_update_callback -- callback used in pmemset_sds_update test
 */
static int
sds_update_callback(struct pmemset *set, struct pmemset_event_context *ctx,
		void *arg)
{
	struct sds_update_args *args = arg;

	if (ctx->type == PMEMSET_EVENT_SDS_UPDATE) {
		UT_ASSERTne(ctx->data.sds_update.sds, NULL);
		UT_ASSERTne(ctx->data.sds_update.src, NULL);
		args->count++;
	}

	return 0;
}

/*
 * test_pmemset_sds_update_event -- test PMEMSET_EVENT_SDS_UPDATE event
 */
static int
test_pmemset_sds_update_event(const struct test_case *tc,
		int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_pmemset_sds_update_event <file>");

	char *file = argv[0];

	struct pmem2_source *pmem2_src;
	struct pmemset *set;
	struct pmemset_config *cfg;
	struct pmemset_map_config *map_cfg;
	struct pmemset_part_map *pmap;
	struct pmemset_sds sds = PMEMSET_SDS_INITIALIZE();
	struct pmemset_sds *sds_copy;
	struct pmemset_source *src;
	struct sds_update_args args;
	args.count = 0;

	int fd = OPEN(file, O_RDWR);

	int ret = pmem2_source_from_fd(&pmem2_src, fd);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_source_from_pmem2(&src, pmem2_src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ut_create_set_config(&cfg);
	pmemset_config_set_event_callback(cfg, &sds_update_callback, &args);

	ret = pmemset_new(&set, cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	pmemset_source_set_sds(src, &sds, NULL);

	ut_create_map_config(&map_cfg, 0, 0);

	ret = pmemset_map(set, src, map_cfg, NULL);
	if (ret == PMEMSET_E_SDS_ENOSUPP)
		goto cleanup;
	UT_ASSERTeq(args.count, 1); /* sds updated, sds refcount raised */

	/* get duplicated SDS (internal function) */
	sds_copy = pmemset_source_get_sds(src);
	/* refcount should be raised */
	UT_ASSERTeq(sds_copy->refcount, 1);

	pmemset_first_part_map(set, &pmap);
	UT_ASSERTne(pmap, NULL);

	ret = pmemset_remove_part_map(set, &pmap);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTeq(pmap, NULL);
	UT_ASSERTeq(args.count, 2); /* sds updated, sds refcount lowered */

	/* refcount should be lowered */
	UT_ASSERTeq(sds_copy->refcount, 0);

cleanup:
	ret = pmemset_map_config_delete(&map_cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	cleanup(set, cfg, src, pmem2_src, fd);
	return 1;
}

/*
 * test_cases -- available test cases
 */
static struct test_case test_cases[] = {
	TEST_CASE(test_pmemset_persist_event),
	TEST_CASE(test_pmemset_copy_event),
	TEST_CASE(test_pmemset_part_add_event),
	TEST_CASE(test_pmemset_part_remove_event),
	TEST_CASE(test_pmemset_remove_range_event),
	TEST_CASE(test_pmemset_sds_update_event),
};

#define NTESTS (sizeof(test_cases) / sizeof(test_cases[0]))

int
main(int argc, char **argv)
{
	START(argc, argv, "pmemset_event");

	TEST_CASE_PROCESS(argc, argv, test_cases, NTESTS);

	DONE(NULL);
}

#ifdef _MSC_VER
MSVC_CONSTR(libpmemset_init)
MSVC_DESTR(libpmemset_fini)
#endif

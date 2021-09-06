// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016-2020, Intel Corporation */

/*
 * util_ctl.c -- tests for the control module
 */

#include "unittest.h"
#include "ctl.h"
#include "out.h"
#include "pmemcommon.h"
#include "fault_injection.h"

#define LOG_PREFIX "ut"
#define LOG_LEVEL_VAR "TEST_LOG_LEVEL"
#define LOG_FILE_VAR "TEST_LOG_FILE"
#define MAJOR_VERSION 1
#define MINOR_VERSION 0

struct pool {
	struct ctl *ctl;
};

static char *testconfig_path;
static int test_config_written;

static int
CTL_READ_HANDLER(test_rw)(void *ctx, enum ctl_query_source source,
	void *arg, struct ctl_indexes *indexes)
{
	UT_ASSERTeq(source, CTL_QUERY_PROGRAMMATIC);

	int *arg_rw = arg;
	*arg_rw = 0;

	return 0;
}

static int
CTL_WRITE_HANDLER(test_rw)(void *ctx, enum ctl_query_source source,
	void *arg, struct ctl_indexes *indexes)
{
	int *arg_rw = arg;
	*arg_rw = 1;
	test_config_written++;

	return 0;
}

static struct ctl_argument CTL_ARG(test_rw) = CTL_ARG_INT;

static int
CTL_WRITE_HANDLER(test_wo)(void *ctx, enum ctl_query_source source,
	void *arg, struct ctl_indexes *indexes)
{
	int *arg_wo = arg;
	*arg_wo = 1;
	test_config_written++;

	return 0;
}

static struct ctl_argument CTL_ARG(test_wo) = CTL_ARG_INT;

#define TEST_CONFIG_VALUE "abcd"

static int
CTL_WRITE_HANDLER(test_config)(void *ctx, enum ctl_query_source source,
	void *arg, struct ctl_indexes *indexes)
{
	UT_ASSERTeq(source, CTL_QUERY_CONFIG_INPUT);

	char *config_value = arg;
	UT_ASSERTeq(strcmp(config_value, TEST_CONFIG_VALUE), 0);
	test_config_written++;

	return 0;
}

static struct ctl_argument CTL_ARG(test_config) = CTL_ARG_STRING(8);

struct complex_arg {
	int a;
	char b[5];
	long long c;
	int d;
};

#define COMPLEX_ARG_TEST_A 12345
#define COMPLEX_ARG_TEST_B "abcd"
#define COMPLEX_ARG_TEST_C 3147483647
#define COMPLEX_ARG_TEST_D 1

static int
CTL_WRITE_HANDLER(test_config_complex_arg)(void *ctx,
	enum ctl_query_source source, void *arg,
	struct ctl_indexes *indexes)
{
	UT_ASSERTeq(source, CTL_QUERY_CONFIG_INPUT);

	struct complex_arg *c = arg;
	UT_ASSERTeq(c->a, COMPLEX_ARG_TEST_A);
	UT_ASSERT(strcmp(COMPLEX_ARG_TEST_B, c->b) == 0);
	UT_ASSERTeq(c->c, COMPLEX_ARG_TEST_C);
	UT_ASSERTeq(c->d, COMPLEX_ARG_TEST_D);
	test_config_written++;

	return 0;
}

static struct ctl_argument CTL_ARG(test_config_complex_arg) = {
	.dest_size = sizeof(struct complex_arg),
	.parsers = {
		CTL_ARG_PARSER_STRUCT(struct complex_arg, a, ctl_arg_integer),
		CTL_ARG_PARSER_STRUCT(struct complex_arg, b, ctl_arg_string),
		CTL_ARG_PARSER_STRUCT(struct complex_arg, c, ctl_arg_integer),
		CTL_ARG_PARSER_STRUCT(struct complex_arg, d, ctl_arg_boolean),
		CTL_ARG_PARSER_END
	}
};

static int
CTL_READ_HANDLER(test_ro)(void *ctx, enum ctl_query_source source,
	void *arg, struct ctl_indexes *indexes)
{
	UT_ASSERTeq(source, CTL_QUERY_PROGRAMMATIC);

	int *arg_ro = arg;
	*arg_ro = 0;

	return 0;
}

static int
CTL_READ_HANDLER(index_value)(void *ctx, enum ctl_query_source source,
	void *arg, struct ctl_indexes *indexes)
{
	UT_ASSERTeq(source, CTL_QUERY_PROGRAMMATIC);

	long *index_value = arg;
	struct ctl_index *idx = PMDK_SLIST_FIRST(indexes);
	UT_ASSERT(strcmp(idx->name, "test_index") == 0);
	*index_value = idx->value;

	return 0;
}

static int
CTL_RUNNABLE_HANDLER(test_runnable)(void *ctx,
	enum ctl_query_source source,
	void *arg, struct ctl_indexes *indexes)
{
	UT_ASSERTeq(source, CTL_QUERY_PROGRAMMATIC);

	int *arg_runnable = arg;
	*arg_runnable = 0;

	return 0;
}

static const struct ctl_node CTL_NODE(test_index)[] = {
	CTL_LEAF_RO(index_value),
	CTL_NODE_END
};

static const struct ctl_node CTL_NODE(debug)[] = {
	CTL_LEAF_RO(test_ro),
	CTL_LEAF_WO(test_wo),
	CTL_LEAF_RUNNABLE(test_runnable),
	CTL_LEAF_RW(test_rw),
	CTL_INDEXED(test_index),
	CTL_LEAF_WO(test_config),
	CTL_LEAF_WO(test_config_complex_arg),

	CTL_NODE_END
};

static int
CTL_WRITE_HANDLER(gtest_config)(void *ctx, enum ctl_query_source source,
	void *arg, struct ctl_indexes *indexes)
{
	UT_ASSERTeq(source, CTL_QUERY_CONFIG_INPUT);

	char *config_value = arg;
	UT_ASSERTeq(strcmp(config_value, TEST_CONFIG_VALUE), 0);
	test_config_written = 1;

	return 0;
}

static struct ctl_argument CTL_ARG(gtest_config) = CTL_ARG_STRING(8);

static int
CTL_READ_HANDLER(gtest_ro)(void *ctx, enum ctl_query_source source,
	void *arg, struct ctl_indexes *indexes)
{
	UT_ASSERTeq(source, CTL_QUERY_PROGRAMMATIC);

	int *arg_ro = arg;
	*arg_ro = 0;

	return 0;
}

static const struct ctl_node CTL_NODE(global_debug)[] = {
	CTL_LEAF_RO(gtest_ro),
	CTL_LEAF_WO(gtest_config),

	CTL_NODE_END
};

static int
util_ctl_get(struct pool *pop, const char *name, void *arg)
{
	LOG(3, "pop %p name %s arg %p", pop, name, arg);
	return ctl_query(pop ? pop->ctl : NULL, pop,
			CTL_QUERY_PROGRAMMATIC, name, CTL_QUERY_READ, arg);
}

static int
util_ctl_set(struct pool *pop, const char *name, void *arg)
{
	LOG(3, "pop %p name %s arg %p", pop, name, arg);
	return ctl_query(pop ? pop->ctl : NULL, pop,
		CTL_QUERY_PROGRAMMATIC, name, CTL_QUERY_WRITE, arg);
}

static int
util_ctl_exec(struct pool *pop, const char *name, void *arg)
{
	LOG(3, "pop %p name %s arg %p", pop, name, arg);
	return ctl_query(pop ? pop->ctl : NULL, pop,
		CTL_QUERY_PROGRAMMATIC, name, CTL_QUERY_RUNNABLE, arg);
}

static void
test_ctl_parser(struct pool *pop)
{
	errno = 0;
	int ret;
	ret = util_ctl_get(pop, NULL, NULL);
	UT_ASSERTne(ret, 0);
	UT_ASSERTne(errno, 0);
	errno = 0;
	ret = util_ctl_get(pop, "a.b.c.d", NULL);
	UT_ASSERTne(ret, 0);
	UT_ASSERTne(errno, 0);
	errno = 0;
	ret = util_ctl_get(pop, "", NULL);
	UT_ASSERTne(ret, 0);
	UT_ASSERTne(errno, 0);
	errno = 0;
	ret = util_ctl_get(pop, "debug.", NULL);
	UT_ASSERTne(ret, 0);
	UT_ASSERTne(errno, 0);
	errno = 0;
	ret = util_ctl_get(pop, ".", NULL);
	UT_ASSERTne(ret, 0);
	UT_ASSERTne(errno, 0);
	errno = 0;
	ret = util_ctl_get(pop, "..", NULL);
	UT_ASSERTne(ret, 0);
	UT_ASSERTne(errno, 0);
	errno = 0;
	ret = util_ctl_get(pop, "1.2.3.4", NULL);
	UT_ASSERTne(ret, 0);
	UT_ASSERTne(errno, 0);
	errno = 0;
	ret = util_ctl_get(pop, "debug.1.", NULL);
	UT_ASSERTne(ret, 0);
	UT_ASSERTne(errno, 0);
	errno = 0;
	ret = util_ctl_get(pop, "debug.1.invalid", NULL);
	UT_ASSERTne(ret, 0);
	UT_ASSERTne(errno, 0);

	/* test methods set read to 0 and write to 1 if successful */

	int arg_read = 1;
	int arg_write = 0;

	errno = 0;

	/* correct name, wrong args */

	ret = util_ctl_get(pop, "debug.test_rw", NULL);
	UT_ASSERTne(ret, 0);
	UT_ASSERTne(errno, 0);
	errno = 0;
	ret = util_ctl_set(pop, "debug.test_rw", NULL);
	UT_ASSERTne(ret, 0);
	UT_ASSERTne(errno, 0);
	errno = 0;
	ret = util_ctl_get(pop, "debug.test_wo", &arg_read);
	UT_ASSERTne(ret, 0);
	UT_ASSERTne(errno, 0);
	errno = 0;
	ret = util_ctl_get(pop, "debug.test_wo", NULL);
	UT_ASSERTne(ret, 0);
	UT_ASSERTne(errno, 0);
	errno = 0;
	ret = util_ctl_set(pop, "debug.test_ro", &arg_write);
	UT_ASSERTne(ret, 0);
	UT_ASSERTne(errno, 0);
	errno = 0;
	ret = util_ctl_set(pop, "debug.test_ro", NULL);
	UT_ASSERTne(ret, 0);
	UT_ASSERTne(errno, 0);
	errno = 0;

	ret = util_ctl_get(pop, "debug.test_rw", &arg_read);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(arg_read, 0);
	UT_ASSERTeq(arg_write, 0);
	UT_ASSERTeq(errno, 0);

	ret = util_ctl_set(pop, "debug.test_rw", &arg_write);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(arg_read, 0);
	UT_ASSERTeq(arg_write, 1);

	arg_read = 1;
	arg_write = 0;

	ret = util_ctl_get(pop, "debug.test_ro", &arg_read);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(arg_read, 0);
	UT_ASSERTeq(arg_write, 0);

	arg_read = 1;
	arg_write = 0;

	ret = util_ctl_set(pop, "debug.test_wo", &arg_write);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(arg_read, 1);
	UT_ASSERTeq(arg_write, 1);

	long index_value = 0;
	ret = util_ctl_get(pop, "debug.5.index_value", &index_value);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(index_value, 5);

	ret = util_ctl_get(pop, "debug.10.index_value", &index_value);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(index_value, 10);

	arg_read = 1;
	arg_write = 1;
	int arg_runnable = 1;

	ret = util_ctl_exec(pop, "debug.test_runnable", &arg_runnable);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(arg_read, 1);
	UT_ASSERTeq(arg_write, 1);
	UT_ASSERTeq(arg_runnable, 0);
}

static void
test_string_config(struct pool *pop)
{
	UT_ASSERTne(pop, NULL);
	int ret;

	test_config_written = 0;
	ret = ctl_load_config_from_string(pop->ctl, pop, "");
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(test_config_written, 0);

	test_config_written = 0;
	ret = ctl_load_config_from_string(pop->ctl, pop, ";;");
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(test_config_written, 0);

	test_config_written = 0;
	ret = ctl_load_config_from_string(pop->ctl, pop, ";=;");
	UT_ASSERTeq(ret, -1);
	UT_ASSERTeq(test_config_written, 0);

	test_config_written = 0;
	ret = ctl_load_config_from_string(pop->ctl, pop, "=");
	UT_ASSERTeq(ret, -1);
	UT_ASSERTeq(test_config_written, 0);

	test_config_written = 0;
	ret = ctl_load_config_from_string(pop->ctl, pop,
			"debug.test_wo=");
	UT_ASSERTeq(ret, -1);
	UT_ASSERTeq(test_config_written, 0);

	test_config_written = 0;
	ret = ctl_load_config_from_string(pop->ctl, pop, "=b");
	UT_ASSERTeq(ret, -1);
	UT_ASSERTeq(test_config_written, 0);

	test_config_written = 0;
	ret = ctl_load_config_from_string(pop->ctl, pop,
			"debug.test_wo=111=222");
	UT_ASSERTeq(ret, -1);
	UT_ASSERTeq(test_config_written, 0);

	test_config_written = 0;
	ret = ctl_load_config_from_string(pop->ctl, pop,
			"debug.test_wo=333;debug.test_rw=444;");
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(test_config_written, 2);

	test_config_written = 0;
	ret = ctl_load_config_from_string(pop->ctl, pop,
			"debug.test_config="TEST_CONFIG_VALUE";");
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(test_config_written, 1);
}

static void
config_file_create(const char *buf)
{
	/* the test script will take care of removing this file for us */
	FILE *f = os_fopen(testconfig_path, "w+");
	fwrite(buf, sizeof(char), strlen(buf), f);
	fclose(f);
}

static void
create_and_test_file_config(struct pool *pop, const char *buf, int ret,
		int result)
{
	config_file_create(buf);

	test_config_written = 0;
	int r = ctl_load_config_from_file(pop ? pop->ctl : NULL,
			pop, testconfig_path);
	UT_ASSERTeq(r, ret);
	UT_ASSERTeq(test_config_written, result);
}

static void
test_too_large_file(struct pool *pop)
{
	char *too_large_buf = calloc(1, 1 << 21);
	UT_ASSERTne(too_large_buf, NULL);
	memset(too_large_buf, 0xc, (1 << 21) - 1);

	config_file_create(too_large_buf);

	int ret = ctl_load_config_from_file(pop->ctl, pop,
			testconfig_path);
	UT_ASSERTne(ret, 0);

	free(too_large_buf);
}

static void
test_file_config(struct pool *pop)
{
	create_and_test_file_config(pop,
		"debug.test_config="TEST_CONFIG_VALUE";", 0, 1);
	create_and_test_file_config(pop,
		"debug.test_config="TEST_CONFIG_VALUE";"
		"debug.test_config="TEST_CONFIG_VALUE";", 0, 2);
	create_and_test_file_config(pop,
		"#this is a comment\n"
		"debug.test_config="TEST_CONFIG_VALUE";", 0, 1);
	create_and_test_file_config(pop,
		"debug.#this is a comment\n"
		"test_config#this is a comment\n"
		"="TEST_CONFIG_VALUE";", 0, 1);
	create_and_test_file_config(pop,
		"debug.test_config="TEST_CONFIG_VALUE";#this is a comment",
		0, 1);
	create_and_test_file_config(pop,
		"\n\n\ndebug\n.\ntest\t_\tconfig="TEST_CONFIG_VALUE";\n", 0, 1);
	create_and_test_file_config(pop,
		" d e b u g . t e s t _ c o n f i g = "TEST_CONFIG_VALUE";",
		0, 1);
	create_and_test_file_config(pop,
		"#debug.test_config="TEST_CONFIG_VALUE";", 0, 0);
	create_and_test_file_config(pop,
		"debug.#this is a comment\n"
		"test_config#this is a not properly terminated comment"
		"="TEST_CONFIG_VALUE";", -1, 0);
	create_and_test_file_config(pop,
		"invalid", -1, 0);
	create_and_test_file_config(pop,
		"", 0, 0);

	create_and_test_file_config(pop,
		"debug.test_config_complex_arg=;", -1, 0);
	create_and_test_file_config(pop,
		"debug.test_config_complex_arg=1,2,3;", -1, 0);
	create_and_test_file_config(pop,
		"debug.test_config_complex_arg=12345,abcd,,1;", -1, 0);
	create_and_test_file_config(pop,
		"debug.test_config_complex_arg=12345,abcd,3147483647,1;", 0, 1);

	create_and_test_file_config(NULL,
		"global_debug.gtest_config="TEST_CONFIG_VALUE";", 0, 1);

	create_and_test_file_config(NULL, "private.missing.query=1;"
		"global_debug.gtest_config="TEST_CONFIG_VALUE";", 0, 1);

	test_too_large_file(pop);

	int ret = ctl_load_config_from_file(pop->ctl,
			pop, "does_not_exist");
	UT_ASSERTne(ret, 0);
}

static void
test_ctl_global_namespace(struct pool *pop)
{
	int arg_read = 1;

	int ret = util_ctl_get(pop, "global_debug.gtest_ro", &arg_read);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(arg_read, 0);
}

static void
test_ctl_arg_parsers()
{
	char *input;

	input = "";
	int boolean = -1;
	int ret = ctl_arg_boolean(input, &boolean, sizeof(int));
	UT_ASSERTeq(ret, -1);
	UT_ASSERTeq(boolean, -1);

	input = "abcdefgh";
	boolean = -1;
	ret = ctl_arg_boolean(input, &boolean, sizeof(int));
	UT_ASSERTeq(ret, -1);
	UT_ASSERTeq(boolean, -1);

	input = "-999";
	boolean = -1;
	ret = ctl_arg_boolean(input, &boolean, sizeof(int));
	UT_ASSERTeq(ret, -1);
	UT_ASSERTeq(boolean, -1);

	input = "N";
	boolean = -1;
	ret = ctl_arg_boolean(input, &boolean, sizeof(int));
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(boolean, 0);

	input = "0";
	boolean = -1;
	ret = ctl_arg_boolean(input, &boolean, sizeof(int));
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(boolean, 0);

	input = "yes";
	boolean = -1;
	ret = ctl_arg_boolean(input, &boolean, sizeof(int));
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(boolean, 1);

	input = "Yes";
	boolean = -1;
	ret = ctl_arg_boolean(input, &boolean, sizeof(int));
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(boolean, 1);

	input = "1";
	boolean = -1;
	ret = ctl_arg_boolean(input, &boolean, sizeof(int));
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(boolean, 1);

	input = "1234";
	boolean = -1;
	ret = ctl_arg_boolean(input, &boolean, sizeof(int));
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(boolean, 1);

	input = "";
	int small_int = -1;
	ret = ctl_arg_integer(input, &small_int, sizeof(small_int));
	UT_ASSERTeq(ret, -1);
	UT_ASSERTeq(small_int, -1);

	input = "abcd";
	small_int = -1;
	ret = ctl_arg_integer(input, &small_int, sizeof(small_int));
	UT_ASSERTeq(ret, -1);
	UT_ASSERTeq(small_int, -1);

	input = "12345678901234567890";
	small_int = -1;
	ret = ctl_arg_integer(input, &small_int, sizeof(small_int));
	UT_ASSERTeq(ret, -1);
	UT_ASSERTeq(small_int, -1);

	input = "-12345678901234567890";
	small_int = -1;
	ret = ctl_arg_integer(input, &small_int, sizeof(small_int));
	UT_ASSERTeq(ret, -1);
	UT_ASSERTeq(small_int, -1);

	input = "2147483648"; /* INT_MAX + 1 */
	small_int = -1;
	ret = ctl_arg_integer(input, &small_int, sizeof(small_int));
	UT_ASSERTeq(ret, -1);
	UT_ASSERTeq(small_int, -1);

	input = "-2147483649"; /* INT_MIN - 2 */
	small_int = -1;
	ret = ctl_arg_integer(input, &small_int, sizeof(small_int));
	UT_ASSERTeq(ret, -1);
	UT_ASSERTeq(small_int, -1);

	input = "0";
	small_int = -1;
	ret = ctl_arg_integer(input, &small_int, sizeof(small_int));
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(small_int, 0);

	input = "500";
	small_int = -1;
	ret = ctl_arg_integer(input, &small_int, sizeof(small_int));
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(small_int, 500);

	input = "-500";
	small_int = -1;
	ret = ctl_arg_integer(input, &small_int, sizeof(small_int));
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(small_int, -500);

	input = "";
	long long ll_int = -1;
	ret = ctl_arg_integer(input, &ll_int, sizeof(ll_int));
	UT_ASSERTeq(ret, -1);
	UT_ASSERTeq(ll_int, -1);

	input = "12345678901234567890";
	ll_int = -1;
	ret = ctl_arg_integer(input, &ll_int, sizeof(ll_int));
	UT_ASSERTeq(ret, -1);
	UT_ASSERTeq(ll_int, -1);

	input = "-12345678901234567890";
	ll_int = -1;
	ret = ctl_arg_integer(input, &ll_int, sizeof(ll_int));
	UT_ASSERTeq(ret, -1);
	UT_ASSERTeq(ll_int, -1);

	input = "2147483648";
	ll_int = -1;
	ret = ctl_arg_integer(input, &ll_int, sizeof(ll_int));
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(ll_int, 2147483648);

	input = "-2147483649";
	ll_int = -1;
	ret = ctl_arg_integer(input, &ll_int, sizeof(ll_int));
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(ll_int, -2147483649LL);

	input = "";
	char string[1000] = {0};
	ret = ctl_arg_string(input, string, 0);
	UT_ASSERTeq(ret, -1);

	input = "abcd";
	ret = ctl_arg_string(input, string, 3);
	UT_ASSERTeq(ret, -1);

	input = "abcdefg";
	ret = ctl_arg_string(input, string, 3);
	UT_ASSERTeq(ret, -1);

	input = "abcd";
	ret = ctl_arg_string(input, string, 4);
	UT_ASSERTeq(ret, -1);

	input = "abc";
	ret = ctl_arg_string(input, string, 4);
	UT_ASSERTeq(ret, 0);
	UT_ASSERT(strcmp(input, string) == 0);
}

static void
test_fault_injection(struct pool *pop)
{
	if (!core_fault_injection_enabled())
		return;

	UT_ASSERTne(pop, NULL);
	core_inject_fault_at(PMEM_MALLOC, 1, "ctl_parse_args");

	test_config_written = 0;
	int ret = ctl_load_config_from_string(pop->ctl, pop,
		"debug.test_wo=333;debug.test_rw=444;");
	UT_ASSERTne(ret, 0);
	UT_ASSERTeq(errno, ENOMEM);
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "util_ctl");

	common_init(LOG_PREFIX, LOG_LEVEL_VAR, LOG_FILE_VAR,
			MAJOR_VERSION, MINOR_VERSION);

	if (argc != 2)
		UT_FATAL("usage: %s testconfig", argv[0]);

	testconfig_path = argv[1];

	CTL_REGISTER_MODULE(NULL, global_debug);

	test_ctl_global_namespace(NULL);

	struct pool pop;

	pop.ctl = ctl_new();

	test_ctl_global_namespace(NULL);

	CTL_REGISTER_MODULE(pop.ctl, debug);

	test_ctl_global_namespace(&pop);

	test_fault_injection(&pop);
	test_ctl_parser(&pop);
	test_string_config(&pop);
	test_file_config(&pop);
	test_ctl_arg_parsers();

	ctl_delete(pop.ctl);

	common_fini();

	DONE(NULL);
}

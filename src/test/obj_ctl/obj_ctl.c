/*
 * Copyright 2016-2018, Intel Corporation
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
 * obj_ctl.c -- tests for the libpmemobj control module
 */

#include "unittest.h"
#include "obj.h"
#include "ctl.h"

static char *testconfig_path;
static int test_config_written;

static int
CTL_READ_HANDLER(test_rw)(PMEMobjpool *pop, enum ctl_query_source source,
	void *arg, struct ctl_indexes *indexes)
{
	UT_ASSERTeq(source, CTL_QUERY_PROGRAMMATIC);

	int *arg_rw = arg;
	*arg_rw = 0;

	return 0;
}

static int
CTL_WRITE_HANDLER(test_rw)(PMEMobjpool *pop, enum ctl_query_source source,
	void *arg, struct ctl_indexes *indexes)
{
	int *arg_rw = arg;
	*arg_rw = 1;
	test_config_written++;

	return 0;
}

struct ctl_argument CTL_ARG(test_rw) = CTL_ARG_INT;

static int
CTL_WRITE_HANDLER(test_wo)(PMEMobjpool *pop, enum ctl_query_source source,
	void *arg, struct ctl_indexes *indexes)
{
	int *arg_wo = arg;
	*arg_wo = 1;
	test_config_written++;

	return 0;
}

struct ctl_argument CTL_ARG(test_wo) = CTL_ARG_INT;

#define TEST_CONFIG_VALUE "abcd"

static int
CTL_WRITE_HANDLER(test_config)(PMEMobjpool *pop, enum ctl_query_source source,
	void *arg, struct ctl_indexes *indexes)
{
	UT_ASSERTeq(source, CTL_QUERY_CONFIG_INPUT);

	char *config_value = arg;
	UT_ASSERTeq(strcmp(config_value, TEST_CONFIG_VALUE), 0);
	test_config_written++;

	return 0;
}

struct ctl_argument CTL_ARG(test_config) = CTL_ARG_STRING(8);

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
CTL_WRITE_HANDLER(test_config_complex_arg)(PMEMobjpool *pop,
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

struct ctl_argument CTL_ARG(test_config_complex_arg) = {
	.sized = 0,
	.dest_size = sizeof(struct complex_arg),
	.parsers = {
		CTL_ARG_PARSER_STRUCT(struct complex_arg, a, ctl_arg_integer),
		CTL_ARG_PARSER_STRUCT(struct complex_arg, b, ctl_arg_string),
		CTL_ARG_PARSER_STRUCT(struct complex_arg, c, ctl_arg_integer),
		CTL_ARG_PARSER_STRUCT(struct complex_arg, d, ctl_arg_boolean),
		CTL_ARG_PARSER_END
	}
};

struct sized_arg {
	size_t size;
	int a;
	int b;
};

static int
CTL_WRITE_HANDLER(sized_arg)(PMEMobjpool *pop,
	enum ctl_query_source source, void *arg,
	struct ctl_indexes *indexes)
{
	struct sized_arg *sarg = arg;

	if (source == CTL_QUERY_CONFIG_INPUT) {
		UT_ASSERT(sarg->size == sizeof(struct sized_arg) ||
			sarg->size == sizeof(struct sized_arg) - sizeof(int));
	} else {
		UT_ASSERTeq(sarg->size, sizeof(struct sized_arg));
	}
	sarg->a = 10;
	sarg->b = 15;
	test_config_written++;

	return 0;
}

struct ctl_argument CTL_ARG(sized_arg) = {
	.sized = 1,
	.dest_size = sizeof(struct sized_arg),
	.parsers = {
		CTL_ARG_PARSER_STRUCT(struct sized_arg, a, ctl_arg_integer),
		CTL_ARG_PARSER_STRUCT_OPTIONAL(struct sized_arg, b,
			ctl_arg_integer),
		CTL_ARG_PARSER_END
	}
};


static int
CTL_READ_HANDLER(test_ro)(PMEMobjpool *pop, enum ctl_query_source source,
	void *arg, struct ctl_indexes *indexes)
{
	UT_ASSERTeq(source, CTL_QUERY_PROGRAMMATIC);

	int *arg_ro = arg;
	*arg_ro = 0;

	return 0;
}

static int
CTL_READ_HANDLER(index_value)(PMEMobjpool *pop, enum ctl_query_source source,
	void *arg, struct ctl_indexes *indexes)
{
	UT_ASSERTeq(source, CTL_QUERY_PROGRAMMATIC);

	long *index_value = arg;
	struct ctl_index *idx = SLIST_FIRST(indexes);
	UT_ASSERT(strcmp(idx->name, "test_index") == 0);
	*index_value = idx->value;

	return 0;
}

static int
CTL_RUNNABLE_HANDLER(test_runnable)(PMEMobjpool *pop,
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
	CTL_LEAF_WO(sized_arg),

	CTL_NODE_END
};

static int
CTL_WRITE_HANDLER(gtest_config)(PMEMobjpool *pop, enum ctl_query_source source,
	void *arg, struct ctl_indexes *indexes)
{
	UT_ASSERTeq(source, CTL_QUERY_CONFIG_INPUT);

	char *config_value = arg;
	UT_ASSERTeq(strcmp(config_value, TEST_CONFIG_VALUE), 0);
	test_config_written = 1;

	return 0;
}

struct ctl_argument CTL_ARG(gtest_config) = CTL_ARG_STRING(8);

static int
CTL_READ_HANDLER(gtest_ro)(PMEMobjpool *pop, enum ctl_query_source source,
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

static void
test_ctl_parser(PMEMobjpool *pop)
{
	errno = 0;
	int ret;
	ret = pmemobj_ctl_get(pop, NULL, NULL);
	UT_ASSERTne(ret, 0);
	UT_ASSERTne(errno, 0);
	errno = 0;
	ret = pmemobj_ctl_get(pop, "a.b.c.d", NULL);
	UT_ASSERTne(ret, 0);
	UT_ASSERTne(errno, 0);
	errno = 0;
	ret = pmemobj_ctl_get(pop, "", NULL);
	UT_ASSERTne(ret, 0);
	UT_ASSERTne(errno, 0);
	errno = 0;
	ret = pmemobj_ctl_get(pop, "debug.", NULL);
	UT_ASSERTne(ret, 0);
	UT_ASSERTne(errno, 0);
	errno = 0;
	ret = pmemobj_ctl_get(pop, ".", NULL);
	UT_ASSERTne(ret, 0);
	UT_ASSERTne(errno, 0);
	errno = 0;
	ret = pmemobj_ctl_get(pop, "..", NULL);
	UT_ASSERTne(ret, 0);
	UT_ASSERTne(errno, 0);
	errno = 0;
	ret = pmemobj_ctl_get(pop, "1.2.3.4", NULL);
	UT_ASSERTne(ret, 0);
	UT_ASSERTne(errno, 0);
	errno = 0;
	ret = pmemobj_ctl_get(pop, "debug.1.", NULL);
	UT_ASSERTne(ret, 0);
	UT_ASSERTne(errno, 0);
	errno = 0;
	ret = pmemobj_ctl_get(pop, "debug.1.invalid", NULL);
	UT_ASSERTne(ret, 0);
	UT_ASSERTne(errno, 0);

	/* test methods set read to 0 and write to 1 if successful */
	int arg_read = 1;
	int arg_write = 0;

	errno = 0;

	/* correct name, wrong args */
	ret = pmemobj_ctl_get(pop, "debug.test_rw", NULL);
	UT_ASSERTne(ret, 0);
	UT_ASSERTne(errno, 0);
	errno = 0;
	ret = pmemobj_ctl_set(pop, "debug.test_rw", NULL);
	UT_ASSERTne(ret, 0);
	UT_ASSERTne(errno, 0);
	errno = 0;
	ret = pmemobj_ctl_get(pop, "debug.test_wo", &arg_read);
	UT_ASSERTne(ret, 0);
	UT_ASSERTne(errno, 0);
	errno = 0;
	ret = pmemobj_ctl_get(pop, "debug.test_wo", NULL);
	UT_ASSERTne(ret, 0);
	UT_ASSERTne(errno, 0);
	errno = 0;
	ret = pmemobj_ctl_set(pop, "debug.test_ro", &arg_write);
	UT_ASSERTne(ret, 0);
	UT_ASSERTne(errno, 0);
	errno = 0;
	ret = pmemobj_ctl_set(pop, "debug.test_ro", NULL);
	UT_ASSERTne(ret, 0);
	UT_ASSERTne(errno, 0);
	errno = 0;

	ret = pmemobj_ctl_get(pop, "debug.test_rw", &arg_read);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(arg_read, 0);
	UT_ASSERTeq(arg_write, 0);
	UT_ASSERTeq(errno, 0);

	ret = pmemobj_ctl_set(pop, "debug.test_rw", &arg_write);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(arg_read, 0);
	UT_ASSERTeq(arg_write, 1);

	arg_read = 1;
	arg_write = 0;

	ret = pmemobj_ctl_get(pop, "debug.test_ro", &arg_read);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(arg_read, 0);
	UT_ASSERTeq(arg_write, 0);

	arg_read = 1;
	arg_write = 0;

	ret = pmemobj_ctl_set(pop, "debug.test_wo", &arg_write);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(arg_read, 1);
	UT_ASSERTeq(arg_write, 1);

	long index_value = 0;
	ret = pmemobj_ctl_get(pop, "debug.5.index_value", &index_value);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(index_value, 5);

	ret = pmemobj_ctl_get(pop, "debug.10.index_value", &index_value);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(index_value, 10);

	arg_read = 1;
	arg_write = 1;
	int arg_runnable = 1;

	ret = pmemobj_ctl_exec(pop, "debug.test_runnable", &arg_runnable);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(arg_read, 1);
	UT_ASSERTeq(arg_write, 1);
	UT_ASSERTeq(arg_runnable, 0);

	struct sized_arg sarg;
	sarg.a = 5;
	sarg.b = 10;
	sarg.size = sizeof(sarg);

	ret = pmemobj_ctl_set(pop, "debug.sized_arg", &sarg);

	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(sarg.a, 10);
	UT_ASSERTeq(sarg.b, 15);
}

static void
test_string_config(PMEMobjpool *pop)
{
	int ret;

	test_config_written = 0;
	ret = ctl_load_config_from_string(pop, "");
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(test_config_written, 0);

	test_config_written = 0;
	ret = ctl_load_config_from_string(pop, ";;");
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(test_config_written, 0);

	test_config_written = 0;
	ret = ctl_load_config_from_string(pop, ";=;");
	UT_ASSERTeq(ret, -1);
	UT_ASSERTeq(test_config_written, 0);

	test_config_written = 0;
	ret = ctl_load_config_from_string(pop, "=");
	UT_ASSERTeq(ret, -1);
	UT_ASSERTeq(test_config_written, 0);

	test_config_written = 0;
	ret = ctl_load_config_from_string(pop, "debug.test_wo=");
	UT_ASSERTeq(ret, -1);
	UT_ASSERTeq(test_config_written, 0);

	test_config_written = 0;
	ret = ctl_load_config_from_string(pop, "=b");
	UT_ASSERTeq(ret, -1);
	UT_ASSERTeq(test_config_written, 0);

	test_config_written = 0;
	ret = ctl_load_config_from_string(pop,
			"debug.test_wo=111=222");
	UT_ASSERTeq(ret, -1);
	UT_ASSERTeq(test_config_written, 0);

	test_config_written = 0;
	ret = ctl_load_config_from_string(pop,
			"debug.test_wo=333;debug.test_rw=444;");
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(test_config_written, 2);

	test_config_written = 0;
	ret = ctl_load_config_from_string(pop,
			"debug.test_config="TEST_CONFIG_VALUE";");
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(test_config_written, 1);

	test_config_written = 0;
	ret = ctl_load_config_from_string(pop,
			"debug.sized_arg=5,10;");
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(test_config_written, 1);

	test_config_written = 0;
	ret = ctl_load_config_from_string(pop,
			"debug.sized_arg=5;");
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
create_and_test_file_config(PMEMobjpool *pop, const char *buf, int ret,
		int result)
{
	config_file_create(buf);

	test_config_written = 0;
	int r = ctl_load_config_from_file(pop, testconfig_path);
	UT_ASSERTeq(r, ret);
	UT_ASSERTeq(test_config_written, result);
}

static void
test_too_large_file(PMEMobjpool *pop)
{
	char *too_large_buf = calloc(1, 1 << 21);
	UT_ASSERTne(too_large_buf, NULL);
	memset(too_large_buf, 0xc, (1 << 21) - 1);

	config_file_create(too_large_buf);

	int ret = ctl_load_config_from_file(pop, testconfig_path);
	UT_ASSERTne(ret, 0);

	free(too_large_buf);
}

static void
test_file_config(PMEMobjpool *pop)
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

	test_too_large_file(pop);

	int ret = ctl_load_config_from_file(pop, "does_not_exist");
	UT_ASSERTne(ret, 0);
}

static void
test_ctl_global_namespace(PMEMobjpool *pop)
{
	int arg_read = 1;

	int ret = pmemobj_ctl_get(pop, "global_debug.gtest_ro", &arg_read);
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

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_ctl");

	if (argc != 3)
		UT_FATAL("usage: %s file-name testconfig", argv[0]);

	const char *path = argv[1];
	testconfig_path = argv[2];

	CTL_REGISTER_MODULE(NULL, global_debug);

	test_ctl_global_namespace(NULL);

	PMEMobjpool *pop;
	if ((pop = pmemobj_create(path, "ctl", PMEMOBJ_MIN_POOL,
		S_IWUSR | S_IRUSR)) == NULL)
		UT_FATAL("!pmemobj_create: %s", path);

	test_ctl_global_namespace(NULL);

	CTL_REGISTER_MODULE(pop->ctl, debug);

	test_ctl_global_namespace(pop);

	test_ctl_parser(pop);
	test_string_config(pop);
	test_file_config(pop);
	test_ctl_arg_parsers();

	pmemobj_close(pop);

	DONE(NULL);
}

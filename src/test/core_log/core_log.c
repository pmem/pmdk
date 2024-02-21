// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2024, Intel Corporation */

/*
 * core_log.c -- unit test to CORE_LOG_...
 */

#undef _GNU_SOURCE
#include <string.h>

#include "unittest.h"
#include "log_internal.h"
#include "last_error_msg.h"


#define NO_ARGS_CONSUMED 0

/*
 * Prevent abort() from CORE_LOG_FATAL()
 * Use core_log_abort() instead.
 * Use core_log_abort() mock to monitor usage of abort() function
 * inside CORE_LOG_FATAL();
 */
#undef abort
#define abort() core_log_abort()
extern void core_log_abort(void);

/* tests */

struct log_function_context {
	enum core_log_level level;
	const char *file_name;
	int line_no;
	const char *function_name;
	char message[8196];
};

static int Log_function_no_of_calls = 0;
static void
log_function(void *context, enum core_log_level level, const char *file_name,
	const int line_no, const char *function_name, const char *message) {
	struct log_function_context *context_in =
		(struct log_function_context *)context;
	UT_ASSERTeq(context_in->level, level);
	UT_ASSERTeq(strcmp(context_in->file_name, file_name), 0);
	UT_ASSERTeq(context_in->line_no, line_no);
	UT_ASSERTeq(strcmp(context_in->function_name, function_name), 0);
	UT_ASSERTeq(strcmp(context_in->message, message), 0);
	UT_ASSERTeq(strlen(context_in->message), strlen(message));
	Log_function_no_of_calls ++;
}

static int Core_log_abort_no_of_calls = 0;
FUNC_MOCK(core_log_abort, void, void)
FUNC_MOCK_RUN_DEFAULT {
	Core_log_abort_no_of_calls++;
}
FUNC_MOCK_END
#define CORE_LOG_UT_MESSAGE "Test message"
#define CORE_LOG_UT_MESSAGE_LONG "Test message long 20Test message long 40" \
"Test message long 60Test message long 80Test message long100" \
"Test message long   Test message long   Test message long160" \
"Test message long   Test message long   Test message long220" \
"Test message long   Test message long   Test message long280" \
"Test message long   Test message long   Test message long340" \
"Test message long   Test message long   Test message long400    407"

#define CORE_LOG_UT_MESSAGE_TOO_LONG CORE_LOG_UT_MESSAGE_LONG \
"Test message long 428"

#define CONCAT2(A, B) A##B

#define CONCAT3(A, B, C) A##B##C

#define TEST_STEP(step_level) \
	context.level = CORE_LOG_LEVEL_##step_level; \
	context.line_no = __LINE__; CONCAT2(CORE_LOG_, step_level) \
	(CORE_LOG_UT_MESSAGE)

static int
test_CORE_LOG_BASIC(const struct test_case *tc, int argc, char *argv[])
{
	struct log_function_context context;
	core_log_set_function(log_function, &context);
	context.file_name = __FILE__;
	context.function_name = __func__;
	strcpy(context.message,CORE_LOG_UT_MESSAGE);
	Core_log_abort_no_of_calls = 0;
	Log_function_no_of_calls = 0;
	TEST_STEP(FATAL);
	UT_ASSERTeq(Core_log_abort_no_of_calls, 1);
	TEST_STEP(ERROR);
	TEST_STEP(WARNING);
	TEST_STEP(NOTICE);
	context.line_no = __LINE__; CORE_LOG_ALWAYS(CORE_LOG_UT_MESSAGE);
	UT_ASSERTeq(Log_function_no_of_calls, 5);
	UT_ASSERTeq(Core_log_abort_no_of_calls, 1);
	return NO_ARGS_CONSUMED;
}

#define TEST_STEP_LONG(step_level) \
	context.level = CORE_LOG_LEVEL_##step_level; \
	context.line_no = __LINE__; CONCAT2(CORE_LOG_, step_level) \
	(CORE_LOG_UT_MESSAGE_LONG)
static int
test_CORE_LOG_BASIC_LONG(const struct test_case *tc, int argc, char *argv[])
{
	struct log_function_context context;
	core_log_set_function(log_function, &context);
	context.file_name = __FILE__;
	context.function_name = __func__;
	strcpy(context.message,CORE_LOG_UT_MESSAGE_LONG);
	Core_log_abort_no_of_calls = 0;
	Log_function_no_of_calls = 0;
	TEST_STEP_LONG(FATAL);
	UT_ASSERTeq(Core_log_abort_no_of_calls, 1);
	TEST_STEP_LONG(ERROR);
	TEST_STEP_LONG(WARNING);
	TEST_STEP_LONG(NOTICE);
	context.line_no = __LINE__ + 1;
	CORE_LOG_ALWAYS(CORE_LOG_UT_MESSAGE_LONG);
	UT_ASSERTeq(Log_function_no_of_calls, 5);
	UT_ASSERTeq(Core_log_abort_no_of_calls, 1);
	return NO_ARGS_CONSUMED;
}

static int
test_CORE_LOG_BASIC_TOO_LONG(const struct test_case *tc, int argc, char *argv[])
{
	struct log_function_context context;
	core_log_set_function(log_function, &context);
	context.file_name = __FILE__;
	context.function_name = __func__;
	strcpy(context.message,CORE_LOG_UT_MESSAGE_LONG);
	Log_function_no_of_calls = 0;
	context.level = CORE_LOG_LEVEL_ERROR;
	context.line_no = __LINE__ + 1;
	CORE_LOG_ERROR(CORE_LOG_UT_MESSAGE_TOO_LONG);
	context.level = CORE_LOG_LEVEL_WARNING;
	context.line_no = __LINE__ + 1;
	CORE_LOG_WARNING(CORE_LOG_UT_MESSAGE_TOO_LONG);
	UT_ASSERTeq(Log_function_no_of_calls, 2);
	return NO_ARGS_CONSUMED;
}

static int
test_CORE_LOG_LAST_BASIC_LONG(const struct test_case *tc, int argc, char *argv[])
{
	struct log_function_context context;
	core_log_set_function(log_function, &context);
	context.file_name = __FILE__;
	context.function_name = __func__;
	strcpy(context.message,CORE_LOG_UT_MESSAGE_LONG);
	context.message[CORE_LAST_ERROR_MSG_MAXPRINT - 1] = '\0';
	Log_function_no_of_calls = 0;
	context.level = CORE_LOG_LEVEL_ERROR;
	context.line_no = __LINE__ + 1;
	CORE_LOG_ERROR_LAST(CORE_LOG_UT_MESSAGE_LONG);
	UT_ASSERTeq(Log_function_no_of_calls, 1);
	return NO_ARGS_CONSUMED;
}

static int
test_CORE_LOG_LAST_BASIC_TOO_LONG(const struct test_case *tc, int argc, char *argv[])
{
	struct log_function_context context;
	//core_log_set_function(log_function, &context);
	context.file_name = __FILE__;
	context.function_name = __func__;
	strcpy(context.message,CORE_LOG_UT_MESSAGE_LONG);
	context.message[CORE_LAST_ERROR_MSG_MAXPRINT - 1] = '\0';
	Log_function_no_of_calls = 0;
	context.level = CORE_LOG_LEVEL_ERROR;
	context.line_no = __LINE__ + 1;
	CORE_LOG_ERROR_LAST(CORE_LOG_UT_MESSAGE_TOO_LONG);
	UT_ASSERTeq(Log_function_no_of_calls, 1);
	return NO_ARGS_CONSUMED;
}

#define CORE_LOG_UT_ERRNO_SHORT 1
#define CORE_LOG_UT_ERRNO_SHORT_STR "Short errno str"

static int
test_CORE_LOG_BASIC_TOO_LONG_W_ERRNO(const struct test_case *tc,
	int argc, char *argv[])
{
	struct log_function_context context;
	core_log_set_function(log_function, &context);
	context.file_name = __FILE__;
	context.function_name = __func__;
	strcpy(context.message,CORE_LOG_UT_MESSAGE_LONG);
	Log_function_no_of_calls = 0;
	errno = CORE_LOG_UT_ERRNO_SHORT;
	context.level = CORE_LOG_LEVEL_ERROR;
	context.line_no = __LINE__ + 1;
	CORE_LOG_ERROR_W_ERRNO(CORE_LOG_UT_MESSAGE_TOO_LONG);
	context.level = CORE_LOG_LEVEL_WARNING;
	context.line_no = __LINE__ + 1;
	CORE_LOG_WARNING_W_ERRNO(CORE_LOG_UT_MESSAGE_TOO_LONG);
	UT_ASSERTeq(Log_function_no_of_calls, 2);
	return NO_ARGS_CONSUMED;
}

#define CORE_LOG_UT_ERRNO_INVALID 2

FUNC_MOCK(__xpg_strerror_r, int, int __errnum, char *__buf, size_t __buflen)
FUNC_MOCK_RUN_DEFAULT {
	switch (__errnum)
	{
	case CORE_LOG_UT_ERRNO_SHORT:
		strcpy(__buf, CORE_LOG_UT_ERRNO_SHORT_STR);
		break;

	case CORE_LOG_UT_ERRNO_INVALID:
		return -1;
		break;

	default:
		break;
	}
	return 0;
}
FUNC_MOCK_END


#define TEST_STEP_W_ERRNO(step_level) \
	context.level = CORE_LOG_LEVEL_##step_level; \
	context.line_no = __LINE__; CONCAT3(CORE_LOG_, step_level, _W_ERRNO) \
	(CORE_LOG_UT_MESSAGE)

static int
test_CORE_LOG_BASIC_W_ERRNO(const struct test_case *tc, int argc, char *argv[])
{
	struct log_function_context context;
	core_log_set_function(log_function, &context);
	context.file_name = __FILE__;
	context.function_name = __func__;
	strcpy(context.message, \
		CORE_LOG_UT_MESSAGE ": " CORE_LOG_UT_ERRNO_SHORT_STR);
	errno = CORE_LOG_UT_ERRNO_SHORT;
	Core_log_abort_no_of_calls = 0;
	Log_function_no_of_calls = 0;
	TEST_STEP_W_ERRNO(FATAL);
	UT_ASSERTeq(Core_log_abort_no_of_calls, 1);
	TEST_STEP_W_ERRNO(ERROR);
	TEST_STEP_W_ERRNO(WARNING);
	UT_ASSERTeq(Log_function_no_of_calls, 3);
	UT_ASSERTeq(Core_log_abort_no_of_calls, 1);
	return NO_ARGS_CONSUMED;
}

static int
test_CORE_LOG_BASIC_W_ERRNO_BAD(const struct test_case *tc, int argc,
	char *argv[])
{
	struct log_function_context context;
	core_log_set_function(log_function, &context);
	context.file_name = __FILE__;
	context.function_name = __func__;
	strcpy(context.message, CORE_LOG_UT_MESSAGE ": ");
	errno = CORE_LOG_UT_ERRNO_INVALID;
	Core_log_abort_no_of_calls = 0;
	Log_function_no_of_calls = 0;
	TEST_STEP_W_ERRNO(FATAL);
	UT_ASSERTeq(Core_log_abort_no_of_calls, 1);
	TEST_STEP_W_ERRNO(ERROR);
	TEST_STEP_W_ERRNO(WARNING);
	UT_ASSERTeq(Log_function_no_of_calls, 3);
	return NO_ARGS_CONSUMED;
}

static struct test_case test_cases[] = {
	TEST_CASE(test_CORE_LOG_BASIC),
	TEST_CASE(test_CORE_LOG_BASIC_LONG),
	TEST_CASE(test_CORE_LOG_BASIC_TOO_LONG),
	TEST_CASE(test_CORE_LOG_LAST_BASIC_LONG),
	TEST_CASE(test_CORE_LOG_LAST_BASIC_TOO_LONG),
	TEST_CASE(test_CORE_LOG_BASIC_TOO_LONG_W_ERRNO),
	TEST_CASE(test_CORE_LOG_BASIC_W_ERRNO),
	TEST_CASE(test_CORE_LOG_BASIC_W_ERRNO_BAD),
};

#define NTESTS ARRAY_SIZE(test_cases)

/* Restore original abort() definition as it is defined in stdlib.h */
#undef abort
extern void abort(void) __THROW __attribute__((__noreturn__));

int
main(int argc, char *argv[])
{
	START(argc, argv, "core_log");

	TEST_CASE_PROCESS(argc, argv, test_cases, NTESTS);
	DONE(NULL);
}

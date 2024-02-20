// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2024, Intel Corporation */

/*
 * core_log.c -- unit test to CORE_LOG_...
 */

#undef _GNU_SOURCE
#include <string.h>
#include <syslog.h>

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
#define abort() core_log_abort()
extern void core_log_abort(void);

/* core_log_abort() - mock for abort() function in CORE_LOG_FATAL() */
static int Core_log_abort_no_of_calls = 0;

FUNC_MOCK(core_log_abort, void, void)
FUNC_MOCK_RUN_DEFAULT {
	Core_log_abort_no_of_calls++;
}
FUNC_MOCK_END

/* strerror_r mock */
#define CORE_LOG_UT_ERRNO_SHORT 1
#define CORE_LOG_UT_ERRNO_SHORT_STR "Short errno str"
#define CORE_LOG_UT_ERRNO_INVALID 2

static int Strerror_r_no_of_calls = 0;

FUNC_MOCK(__xpg_strerror_r, int, int __errnum, char *__buf, size_t __buflen)
FUNC_MOCK_RUN_DEFAULT {
	Strerror_r_no_of_calls ++;
	switch (__errnum)
	{
	case CORE_LOG_UT_ERRNO_SHORT:
		return snprintf(__buf, __buflen, CORE_LOG_UT_ERRNO_SHORT_STR);
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

static int Syslog_no_of_calls = 0;

FUNC_MOCK(syslog, void, int __pri, const char *__fmt, ...)
FUNC_MOCK_RUN_DEFAULT {
	char buf[4096];
	va_list arg;
	va_start(arg, __fmt);
	vsnprintf(buf, 4096, __fmt, arg);
	va_end(arg);
	__real_syslog(__pri, buf);
	Syslog_no_of_calls ++;
}
FUNC_MOCK_END

static int Fprintf_no_of_calls = 0;

FUNC_MOCK(fprintf, int, FILE *__restrict __stream,
	const char *__restrict __fmt, ...)
FUNC_MOCK_RUN_DEFAULT {
	char buf[4096];
	va_list arg;
	va_start(arg, __fmt);
	vsnprintf(buf, 4096, __fmt, arg);
	va_end(arg);
	Fprintf_no_of_calls ++;
	return __real_fprintf(__stream, buf);
}
FUNC_MOCK_END

#define CORE_LOG_UT_MESSAGE "Test message"

#define CORE_LOG_UT_MESSAGE_LONG "Test message long 20Test message long 40" \
"Test message long 60Test message long 80Test message long100" \
"Test message long120Test message long140Test message long160" \
"Test message long180Test message long200Test message long220" \
"Test message long240Test message long260Test message long280" \
"Test message long300Test message long320Test message long340" \
"Test message long360Test message long380Test message long400    407"

#define CORE_LOG_UT_MESSAGE_TOO_LONG CORE_LOG_UT_MESSAGE_LONG \
"Test message long 428"

#define TEST_SETUP(message_to_test) \
	struct log_function_context context; \
	core_log_set_function(log_function, &context); \
	context.file_name = __FILE__; \
	context.function_name = __func__; \
	Core_log_abort_no_of_calls = 0; \
	Log_function_no_of_calls = 0; \
	strcpy(context.message, message_to_test)

#define CONCAT2(A, B) A##B
#define CONCAT3(A, B, C) A##B##C

#define TEST_STEP(step_level) \
	context.level = CORE_LOG_LEVEL_##step_level; \
	context.line_no = __LINE__; CONCAT2(CORE_LOG_, step_level) \
	(CORE_LOG_UT_MESSAGE)

#define TEST_STEP_LONG(step_level) \
	context.level = CORE_LOG_LEVEL_##step_level; \
	context.line_no = __LINE__; CONCAT2(CORE_LOG_, step_level) \
	(CORE_LOG_UT_MESSAGE_LONG)

#define TEST_STEP_W_ERRNO(step_level) \
	context.level = CORE_LOG_LEVEL_##step_level; \
	context.line_no = __LINE__; CONCAT3(CORE_LOG_, step_level, _W_ERRNO) \
	(CORE_LOG_UT_MESSAGE)

/* tests */

/* structure for test context to be delivered to log_function */
struct log_function_context {
	enum core_log_level level;
	const char *file_name;
	int line_no;
	const char *function_name;
	char message[8196];
};

/* log_function - the main place for log message correctnes verification */
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

static int
test_CORE_LOG_BASIC(const struct test_case *tc, int argc, char *argv[])
{
	TEST_SETUP(CORE_LOG_UT_MESSAGE);
	TEST_STEP(FATAL);
	UT_ASSERTeq(Core_log_abort_no_of_calls, 1);
	TEST_STEP(ERROR);
	TEST_STEP(WARNING);
	TEST_STEP(NOTICE);
	TEST_STEP(HARK);
	UT_ASSERTeq(Log_function_no_of_calls, 5);
	UT_ASSERTeq(Core_log_abort_no_of_calls, 1);
	return NO_ARGS_CONSUMED;
}

static int
test_CORE_LOG_BASIC_LONG(const struct test_case *tc, int argc, char *argv[])
{
	TEST_SETUP(CORE_LOG_UT_MESSAGE_LONG);
	TEST_STEP_LONG(FATAL);
	UT_ASSERTeq(Core_log_abort_no_of_calls, 1);
	TEST_STEP_LONG(ERROR);
	TEST_STEP_LONG(WARNING);
	TEST_STEP_LONG(NOTICE);
	TEST_STEP_LONG(HARK);
	UT_ASSERTeq(Log_function_no_of_calls, 5);
	UT_ASSERTeq(Core_log_abort_no_of_calls, 1);
	return NO_ARGS_CONSUMED;
}

static int
test_CORE_LOG_BASIC_TOO_LONG(const struct test_case *tc, int argc, char *argv[])
{
	TEST_SETUP(CORE_LOG_UT_MESSAGE_LONG);
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
test_CORE_LOG_LAST_BASIC_LONG(const struct test_case *tc, int argc,
	char *argv[])
{
	TEST_SETUP(CORE_LOG_UT_MESSAGE_LONG);
	context.message[CORE_LAST_ERROR_MSG_MAXPRINT - 1] = '\0';
	context.level = CORE_LOG_LEVEL_ERROR;
	context.line_no = __LINE__ + 1;
	CORE_LOG_ERROR_LAST(CORE_LOG_UT_MESSAGE_LONG);
	UT_ASSERTeq(Log_function_no_of_calls, 1);
	return NO_ARGS_CONSUMED;
}

static int
test_CORE_LOG_LAST_BASIC_TOO_LONG(const struct test_case *tc, int argc,
	char *argv[])
{
	TEST_SETUP(CORE_LOG_UT_MESSAGE_LONG);
	context.message[CORE_LAST_ERROR_MSG_MAXPRINT - 1] = '\0';
	context.level = CORE_LOG_LEVEL_ERROR;
	context.line_no = __LINE__ + 1;
	CORE_LOG_ERROR_LAST(CORE_LOG_UT_MESSAGE_TOO_LONG);
	UT_ASSERTeq(Log_function_no_of_calls, 1);
	return NO_ARGS_CONSUMED;
}

static int
test_CORE_LOG_BASIC_TOO_LONG_W_ERRNO(const struct test_case *tc,
	int argc, char *argv[])
{
	TEST_SETUP(CORE_LOG_UT_MESSAGE_LONG);
	errno = CORE_LOG_UT_ERRNO_SHORT;
	context.level = CORE_LOG_LEVEL_ERROR;
	context.line_no = __LINE__ + 1;
	CORE_LOG_ERROR_W_ERRNO(CORE_LOG_UT_MESSAGE_TOO_LONG);
	context.level = CORE_LOG_LEVEL_WARNING;
	context.line_no = __LINE__ + 1;
	CORE_LOG_WARNING_W_ERRNO(CORE_LOG_UT_MESSAGE_TOO_LONG);
	UT_ASSERTeq(errno, CORE_LOG_UT_ERRNO_SHORT);
	UT_ASSERTeq(Log_function_no_of_calls, 2);
	return NO_ARGS_CONSUMED;
}

static int
test_CORE_LOG_BASIC_W_ERRNO(const struct test_case *tc, int argc, char *argv[])
{
	TEST_SETUP(CORE_LOG_UT_MESSAGE ": " CORE_LOG_UT_ERRNO_SHORT_STR);
	errno = CORE_LOG_UT_ERRNO_SHORT;
	TEST_STEP_W_ERRNO(FATAL);
	UT_ASSERTeq(Core_log_abort_no_of_calls, 1);
	TEST_STEP_W_ERRNO(ERROR);
	TEST_STEP_W_ERRNO(WARNING);
	UT_ASSERTeq(Log_function_no_of_calls, 3);
	UT_ASSERTeq(Core_log_abort_no_of_calls, 1);
	UT_ASSERTeq(errno, CORE_LOG_UT_ERRNO_SHORT);
	return NO_ARGS_CONSUMED;
}

static int
test_CORE_LOG_BASIC_W_ERRNO_BAD(const struct test_case *tc, int argc,
	char *argv[])
{
	TEST_SETUP(CORE_LOG_UT_MESSAGE ": ");
	errno = CORE_LOG_UT_ERRNO_INVALID;
	TEST_STEP_W_ERRNO(FATAL);
	UT_ASSERTeq(Core_log_abort_no_of_calls, 1);
	TEST_STEP_W_ERRNO(ERROR);
	TEST_STEP_W_ERRNO(WARNING);
	UT_ASSERTeq(Log_function_no_of_calls, 3);
	return NO_ARGS_CONSUMED;
}

#define CORE_LOG_TRESHOLD_STEP(_call_type, _abort_no, _syslog_no, _fprintf_no) \
	do { \
		Syslog_no_of_calls = 0; \
		Fprintf_no_of_calls = 0; \
		_call_type(CORE_LOG_UT_MESSAGE); \
		UT_ASSERTeq(Core_log_abort_no_of_calls, _abort_no); \
		UT_ASSERTeq(Syslog_no_of_calls, _syslog_no); \
		UT_ASSERTeq(Fprintf_no_of_calls, _fprintf_no); \
	} while (0)

#define CORE_LOG_TRESHOLD_STEP_ALL(_fs, _fe, _es, _ee, _ws, _we, _ns, _ne, \
	_as) \
	Core_log_abort_no_of_calls = 0; \
	CORE_LOG_TRESHOLD_STEP(CORE_LOG_FATAL, 1, _fs, _fe); \
	Core_log_abort_no_of_calls = 0; \
	CORE_LOG_TRESHOLD_STEP(CORE_LOG_ERROR, 0, _es, _es); \
	CORE_LOG_TRESHOLD_STEP(CORE_LOG_WARNING, 0, _ws, _we); \
	CORE_LOG_TRESHOLD_STEP(CORE_LOG_NOTICE, 0, _ns, _ne); \
	CORE_LOG_TRESHOLD_STEP(CORE_LOG_HARK, 0, _as, 0);

static int
test_CORE_LOG_TRESHOLD(const struct test_case *tc, int argc, char *argv[])
{
	Core_log_abort_no_of_calls = 0;
	core_log_set_function(CORE_LOG_USE_DEFAULT_FUNCTION, NULL);
	CORE_LOG_TRESHOLD_STEP(CORE_LOG_FATAL, 1, 1, 1);
	Core_log_abort_no_of_calls = 0;
	CORE_LOG_TRESHOLD_STEP(CORE_LOG_ERROR, 0, 1, 1);
	CORE_LOG_TRESHOLD_STEP(CORE_LOG_WARNING, 0, 1, 1);
	CORE_LOG_TRESHOLD_STEP(CORE_LOG_NOTICE, 0, 1, 0);
	CORE_LOG_TRESHOLD_STEP(CORE_LOG_HARK, 0, 1, 0);
	core_log_set_threshold(CORE_LOG_THRESHOLD, CORE_LOG_LEVEL_HARK);
	CORE_LOG_TRESHOLD_STEP_ALL(0, 0, 0, 0, 0, 0, 0, 0, 1);
	core_log_set_threshold(CORE_LOG_THRESHOLD, CORE_LOG_LEVEL_FATAL);
	CORE_LOG_TRESHOLD_STEP_ALL(1, 1, 0, 0, 0, 0, 0, 0, 1);
	core_log_set_threshold(CORE_LOG_THRESHOLD, CORE_LOG_LEVEL_ERROR);
	CORE_LOG_TRESHOLD_STEP_ALL(1, 1, 1, 1, 0, 0, 0, 0, 1);
	core_log_set_threshold(CORE_LOG_THRESHOLD, CORE_LOG_LEVEL_WARNING);
	CORE_LOG_TRESHOLD_STEP_ALL(1, 1, 1, 1, 1, 1, 0, 0, 1);
	core_log_set_threshold(CORE_LOG_THRESHOLD, CORE_LOG_LEVEL_NOTICE);
	CORE_LOG_TRESHOLD_STEP_ALL(1, 1, 1, 1, 1, 1, 1, 0, 1);
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
	TEST_CASE(test_CORE_LOG_TRESHOLD),
};

#define NTESTS ARRAY_SIZE(test_cases)

/* Restore original abort() definition as it is defined in stdlib.h */
#undef abort
extern void abort(void) __THROW __attribute__((__noreturn__));

#undef LOG_SET_PMEMCORE_FUNC
#define LOG_SET_PMEMCORE_FUNC

int
main(int argc, char *argv[])
{
	START(argc, argv, "core_log");
	openlog("core_log", 0, 0);

	TEST_CASE_PROCESS(argc, argv, test_cases, NTESTS);
	closelog();
	DONE(NULL);
}

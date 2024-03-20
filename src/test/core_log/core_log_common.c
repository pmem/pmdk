// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2024, Intel Corporation */

/*
 * core_log_common.c -- mocks and helpers common for core_log(_no_func)
 */

#include "last_error_msg.h"
#include "unittest.h"

#include "core_log_common.h"

/* mocks */

struct common_ctx Common;
struct vsnprintf_ctx Vsnprintf_;
struct strerror_r_ctx Strerror_r;
struct log_function_ctx Log_function_;

FUNC_MOCK_NONSTATIC(last_error_msg_get, const char *, void)
FUNC_MOCK_RUN_DEFAULT {
	return LAST_ERROR_MSG_MOCK;
}
FUNC_MOCK_END

FUNC_MOCK_NONSTATIC(vsnprintf, int, char *__restrict __s, size_t __maxlen,
	const char *__restrict __format, va_list __arg)
	FUNC_MOCK_RUN(VALIDATED_CALL) {
		if (Common.use_last_error_msg) {
			UT_ASSERTeq(__s, LAST_ERROR_MSG_MOCK);
			UT_ASSERTeq(__maxlen, CORE_LAST_ERROR_MSG_MAXPRINT);
		} else {
			UT_ASSERTne(__s, LAST_ERROR_MSG_MOCK);
			UT_ASSERTeq(__maxlen, _CORE_LOG_MSG_MAXPRINT);
		}
		UT_ASSERT(__format == MSG_FORMAT);
		(void) __arg;

		return Vsnprintf_.ret;
	}
FUNC_MOCK_RUN_DEFAULT {
	return _FUNC_REAL(vsnprintf)(__s, __maxlen, __format, __arg);
}
FUNC_MOCK_END

FUNC_MOCK_NONSTATIC(__xpg_strerror_r, int, int __errnum, char *__buf,
	size_t __buflen)
FUNC_MOCK_RUN_DEFAULT {
	UT_ASSERTeq(__errnum, DUMMY_ERRNO1);
	UT_ASSERTeq(__buf, Strerror_r.exp__buf);
	UT_ASSERTeq(__buflen, Strerror_r.exp__buflen);

	if (Strerror_r.error == EXIT_SUCCESS) {
		return 0;
	}

	if (Strerror_r.before_glibc_2_13) {
		errno = Strerror_r.error;
		return -1;
	} else {
		return Strerror_r.error;
	}
}
FUNC_MOCK_END

FUNC_MOCK_NONSTATIC(core_log_default_function, void, enum core_log_level level,
	const char *file_name, unsigned line_no, const char *function_name,
	const char *message)
	FUNC_MOCK_RUN(VALIDATED_CALL) {
		UT_ASSERTeq(level, Log_function_.exp_level);
		UT_ASSERTstreq(file_name, FILE_NAME);
		UT_ASSERTeq(line_no, LINE_NO);
		UT_ASSERTstreq(function_name, FUNC_NAME);
		if (Common.use_last_error_msg) {
			UT_ASSERTeq(message, LAST_ERROR_MSG_MOCK);
		} else {
			UT_ASSERTne(message, LAST_ERROR_MSG_MOCK);
		}
		return;
	}
FUNC_MOCK_RUN_DEFAULT {
	_FUNC_REAL(core_log_default_function)(level, file_name, line_no,
		function_name, message);
}
FUNC_MOCK_END

FUNC_MOCK_NONSTATIC(custom_log_function, void, enum core_log_level level,
	const char *file_name, unsigned line_no, const char *function_name,
	const char *message)
	FUNC_MOCK_RUN(VALIDATED_CALL) {
		UT_ASSERTeq(level, Log_function_.exp_level);
		UT_ASSERTstreq(file_name, FILE_NAME);
		UT_ASSERTeq(line_no, LINE_NO);
		UT_ASSERTstreq(function_name, FUNC_NAME);
		if (Common.use_last_error_msg) {
			UT_ASSERTeq(message, LAST_ERROR_MSG_MOCK);
		} else {
			UT_ASSERTne(message, LAST_ERROR_MSG_MOCK);
		}
		return;
	}
FUNC_MOCK_RUN_DEFAULT {
	_FUNC_REAL(custom_log_function)(level, file_name, line_no,
		function_name, message);
}
FUNC_MOCK_END

void
custom_log_function(enum core_log_level level, const char *file_name,
	unsigned line_no, const char *function_name, const char *message)
{
	SUPPRESS_UNUSED(level, file_name, line_no, function_name, message);
}

/* helpers */

void
reset_mocks(void)
{
	FUNC_MOCK_RCOUNTER_SET(last_error_msg_get, VALIDATED_CALL);
	FUNC_MOCK_RCOUNTER_SET(vsnprintf, VALIDATED_CALL);
	FUNC_MOCK_RCOUNTER_SET(__xpg_strerror_r, VALIDATED_CALL);
	FUNC_MOCK_RCOUNTER_SET(core_log_default_function, VALIDATED_CALL);
	FUNC_MOCK_RCOUNTER_SET(custom_log_function, VALIDATED_CALL);
}

void
test_no_space_for_strerror_r_helper(int core_message_length)
{
	reset_mocks();

	/* set the expectations */
	Vsnprintf_.ret = core_message_length;
	Log_function_.exp_level = CORE_LOG_LEVEL_ERROR;
	Common.use_last_error_msg = false;

	core_log(CORE_LOG_LEVEL_ERROR, DUMMY_ERRNO1, FILE_NAME, LINE_NO,
		FUNC_NAME, MSG_FORMAT);

	/* check the call counters */
	UT_ASSERTeq(RCOUNTER(last_error_msg_get), NOT_CALLED);
	UT_ASSERTeq(RCOUNTER(vsnprintf), CALLED);
	UT_ASSERTeq(RCOUNTER(__xpg_strerror_r), NOT_CALLED);
	UT_ASSERTeq(RCOUNTER(core_log_default_function), CALLED);
}

void
test_strerror_r_fail_helper(bool before_glibc_2_13)
{
	reset_mocks();

	/* set the expectations */
	Vsnprintf_.ret = BASIC_MESSAGE_LEN;
	Log_function_.exp_level = CORE_LOG_LEVEL_ERROR;
	Common.use_last_error_msg = true;
	Strerror_r.exp__buf = LAST_ERROR_MSG_MOCK + Vsnprintf_.ret;
	Strerror_r.exp__buflen = CORE_LAST_ERROR_MSG_MAXPRINT -
		(size_t)Vsnprintf_.ret;
	Strerror_r.error = DUMMY_ERRNO2;
	Strerror_r.before_glibc_2_13 = before_glibc_2_13;

	core_log(CORE_LOG_LEVEL_ERROR_LAST, DUMMY_ERRNO1, FILE_NAME, LINE_NO,
		FUNC_NAME, MSG_FORMAT);

	/* check the call counters */
	UT_ASSERTeq(RCOUNTER(last_error_msg_get), CALLED);
	UT_ASSERTeq(RCOUNTER(vsnprintf), CALLED);
	UT_ASSERTeq(RCOUNTER(__xpg_strerror_r), CALLED);
	UT_ASSERTeq(RCOUNTER(core_log_default_function), CALLED);
}

void
test_log_function_call_helper(enum core_log_level level,
	bool log_function_called)
{
	reset_mocks();

	/* set the expectations */
	Vsnprintf_.ret = BASIC_MESSAGE_LEN;
	Log_function_.exp_level = level;
	Common.use_last_error_msg = (level == CORE_LOG_LEVEL_ERROR_LAST);

	core_log(level, NO_ERRNO, FILE_NAME, LINE_NO, FUNC_NAME, MSG_FORMAT);

	/* check the call counters */
	UT_ASSERTeq(RCOUNTER(last_error_msg_get),
		Common.use_last_error_msg ? CALLED : NOT_CALLED);
	UT_ASSERTeq(RCOUNTER(vsnprintf), CALLED);
	UT_ASSERTeq(RCOUNTER(__xpg_strerror_r), NOT_CALLED);
	UT_ASSERTeq(RCOUNTER(core_log_default_function),
		log_function_called ? CALLED : NOT_CALLED);
}

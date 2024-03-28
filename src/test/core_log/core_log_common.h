/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2024, Intel Corporation */

/*
 * core_log.c -- definitions for mocks and helpers common for core_log(_no_func)
 */

#include <stdbool.h>

#include "unittest.h"

#define NO_ARGS_CONSUMED 0

#define FILE_NAME "dummy.c"
#define LINE_NO 1234
#define FUNC_NAME "dummy_func"
#define MSG_FORMAT ((char *)0x0458f044)
#define LAST_ERROR_MSG_MOCK ((char *)0x1a547e58)
#define DUMMY_ERRNO1 500
#define DUMMY_ERRNO2 (DUMMY_ERRNO1 + 1)
#define BASIC_MESSAGE_LEN 131

#define VALIDATED_CALL 127
#define NOT_CALLED VALIDATED_CALL
#define CALLED (VALIDATED_CALL + 1)
#define NOT_VALIDATED_CALL 0

extern struct common_ctx {
	bool use_last_error_msg;
} Common;

extern struct vsnprintf_ctx {
	int ret;
} Vsnprintf_;

extern struct strerror_r_ctx {
	char *exp__buf;
	size_t exp__buflen;
	bool before_glibc_2_13;
	int error;
} Strerror_r;

extern struct log_function_ctx {
	enum core_log_level exp_level;
} Log_function_;

/* mocks */

FUNC_MOCK_EXTERN(last_error_msg_get);
FUNC_MOCK_EXTERN(vsnprintf);
FUNC_MOCK_EXTERN(__xpg_strerror_r);
FUNC_MOCK_EXTERN(core_log_default_function);
FUNC_MOCK_EXTERN(custom_log_function);

/* helpers */

void reset_mocks(void);

void test_no_space_for_strerror_r_helper(int core_message_length);

void test_strerror_r_fail_helper(bool before_glibc_2_13);

void test_log_function_call_helper(enum core_log_level level,
	bool call_log_function);

void custom_log_function(enum core_log_level level, const char *file_name,
	unsigned line_no, const char *function_name, const char *message);

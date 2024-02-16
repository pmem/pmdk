// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020-2024, Intel Corporation */

/*
 * log.c -- support for logging output to either syslog or stderr or
 * via user defined function
 */

#include <stdarg.h>
#include <syslog.h>
#include <time.h>
#include <errno.h>
#ifdef ATOMIC_OPERATIONS_SUPPORTED
#include <stdatomic.h>
#endif /* ATOMIC_OPERATIONS_SUPPORTED */
#include <string.h>
#include <stdio.h>

#include "log_internal.h"
#include "log_default.h"
#include "last_error_msg.h"
#include "core_assert.h"

/*
 * Default levels of the logging thresholds
 */
#ifdef DEBUG
#define CORE_LOG_THRESHOLD_DEFAULT CORE_LOG_LEVEL_DEBUG
#define CORE_LOG_THRESHOLD_AUX_DEFAULT CORE_LOG_LEVEL_WARNING
#else
#define CORE_LOG_THRESHOLD_DEFAULT CORE_LOG_LEVEL_WARNING
#define CORE_LOG_THRESHOLD_AUX_DEFAULT CORE_LOG_DISABLED
#endif

/*
 * Core_log_function -- pointer to the logging function saved as uintptr_t to
 * make it _Atomic, because function pointers cannot be _Atomic. By default it
 * is core_log_default_function(), but could be a user-defined logging function
 * provided via core_log_set_function().
 */
#ifdef ATOMIC_OPERATIONS_SUPPORTED
_Atomic
#endif /* ATOMIC_OPERATIONS_SUPPORTED */
uintptr_t Core_log_function = 0;

/* the logging function's context */
#ifdef ATOMIC_OPERATIONS_SUPPORTED
_Atomic
#endif /* ATOMIC_OPERATIONS_SUPPORTED */
void *Core_log_function_context;

/* threshold levels */
#ifdef ATOMIC_OPERATIONS_SUPPORTED
_Atomic
#endif /* ATOMIC_OPERATIONS_SUPPORTED */
enum core_log_level Core_log_threshold[] = {
		CORE_LOG_THRESHOLD_DEFAULT,
		CORE_LOG_THRESHOLD_AUX_DEFAULT
};

/*
 * core_log_init -- initialize and set the default logging function
 */
void
core_log_init()
{
	/*
	 * The core log might be already initialized.
	 * It might happen in the case of some unit tests.
	 */
	if (Core_log_function != 0)
		return;

	/* enable the default logging function */
	core_log_default_init();
	while (EAGAIN ==
		core_log_set_function(CORE_LOG_USE_DEFAULT_FUNCTION, NULL))
		;
}

/*
 * core_log_fini -- disable logging and cleanup the default logging function
 */
void
core_log_fini()
{
	/*
	 * NULL-ed function pointer turns off the logging. No matter if
	 * the previous value was the default logging function or a user
	 * logging function.
	 */
	Core_log_function = 0;
	Core_log_function_context = NULL;

	/* cleanup the default logging function */
	core_log_default_fini();
}

/*
 * core_log_set_function -- set the log function pointer either to
 * a user-provided function pointer or to the default logging function.
 */
int
core_log_set_function(core_log_function *log_function, void *context)
{

	if (log_function == CORE_LOG_USE_DEFAULT_FUNCTION)
		log_function = core_log_default_function;

#ifdef ATOMIC_OPERATIONS_SUPPORTED
	atomic_store_explicit(&Core_log_function, (uintptr_t)log_function,
		__ATOMIC_SEQ_CST);
	atomic_store_explicit(&Core_log_function_context, context,
		__ATOMIC_SEQ_CST);
	return 0;
#else
	uintptr_t core_log_function_old = Core_log_function;
	void *context_old = Core_log_function_context;
	if (!__sync_bool_compare_and_swap(&Core_log_function,
			core_log_function_old, (uintptr_t)log_function))
		return EAGAIN;
	if (__sync_bool_compare_and_swap(&Core_log_function_context,
			context_old, context))
		return 0;

	__sync_bool_compare_and_swap(&Core_log_function,
		(uintptr_t)log_function, core_log_function_old);
	return EAGAIN;

#endif /* ATOMIC_OPERATIONS_SUPPORTED */
}

/*
 * core_log_set_threshold -- set the log level threshold
 */
int
core_log_set_threshold(enum core_log_threshold threshold,
		enum core_log_level level)
{
	if (threshold != CORE_LOG_THRESHOLD &&
			threshold != CORE_LOG_THRESHOLD_AUX)
		return EINVAL;

	if (level < CORE_LOG_DISABLED || level > CORE_LOG_LEVEL_DEBUG)
		return EINVAL;

#ifdef ATOMIC_OPERATIONS_SUPPORTED
	atomic_store_explicit(&Log_threshold[threshold], level,
		__ATOMIC_SEQ_CST);
	return 0;
#else
	enum core_log_level level_old;
	while (EAGAIN == core_log_get_threshold(threshold, &level_old))
		;

	if (__sync_bool_compare_and_swap(&Core_log_threshold[threshold],
			level_old, level))
		return 0;
	else
		return EAGAIN;
#endif /* ATOMIC_OPERATIONS_SUPPORTED */
}

/*
 * core_log_get_threshold -- get the log level threshold
 */
int
core_log_get_threshold(enum core_log_threshold threshold,
	enum core_log_level *level)
{
	if (threshold != CORE_LOG_THRESHOLD &&
			threshold != CORE_LOG_THRESHOLD_AUX)
		return EINVAL;

	if (level == NULL)
		return EINVAL;

#ifdef ATOMIC_OPERATIONS_SUPPORTED
	*level = atomic_load_explicit(&Log_threshold[threshold],
		__ATOMIC_SEQ_CST);
#else
	*level = Core_log_threshold[threshold];
#endif /* ATOMIC_OPERATIONS_SUPPORTED */

	return 0;
}

/*
 * Required to handle both variants' return types.
 * The ideal solution would be to force using one variant or another.
 */
#ifdef _GNU_SOURCE
#define _CORE_LOG_STRERROR_R(buf, buf_len, out) \
	do { \
		char *ret = strerror_r(errno, (buf), (buf_len)); \
		*(out) = ret; \
	} while (0)
#else
#define _CORE_LOG_STRERROR_R(buf, buf_len, out) \
	do { \
		int ret = strerror_r(errno, (buf), (buf_len)); \
		(void) ret; \
		*(out) = buf; \
	} while (0)
#endif

#define _CORE_LOG_STRERROR(buf, buf_len, out) \
	do { \
		int oerrno = errno; \
		_CORE_LOG_STRERROR_R((buf), (buf_len), (out)); \
		errno = oerrno; \
	} while (0)

static void inline
core_log_va(char *buf, size_t buf_len, enum core_log_level level,
	int use_errno, const char *file_name, int line_no,
	const char *function_name, const char *message_format, va_list arg)
{
	int msg_len = 0;
	msg_len = vsnprintf(buf, buf_len, message_format, arg);

	if (msg_len < 0) {
		return;
	}

	if (use_errno) {
		char *msg_ptr, *error_str;
		msg_ptr = buf + msg_len;
		msg_len += 2;
		if (msg_len >= (int)buf_len) {
			return;
		}
		*msg_ptr++ = ':';
		*msg_ptr++ = ' ';
		*msg_ptr = '\0';

		ASSERT(msg_len + _CORE_LOG_MAX_ERRNO_MSG < (int)buf_len);
		_CORE_LOG_STRERROR(msg_ptr, _CORE_LOG_MAX_ERRNO_MSG,
			&error_str);
		ASSERT(msg_ptr == error_str);
	}

	/*
	 * Despite this check is already done when the function is called from
	 * the CORE_LOG() macro it has to be done here again since it is not
	 * performed in the case of the CORE_LOG_TO_LAST macro. Sorry.
	 */
	if (level > Core_log_threshold[CORE_LOG_THRESHOLD]) {
		return;
	}

	if (0 == Core_log_function) {
		return;
	}

	((core_log_function *)Core_log_function)(Core_log_function_context,
		level, file_name, line_no, function_name, buf);
}

void
core_log(enum core_log_level level, int use_errno, const char *file_name,
	int line_no, const char *function_name, const char *message_format, ...)
{
	char message[1024] = "";
	va_list arg;

	va_start(arg, message_format);
	core_log_va(message, sizeof(message), level, use_errno, file_name,
		line_no, function_name, message_format, arg);
	va_end(arg);
}

void
core_log_to_last(int use_errno, const char *file_name, int line_no,
	const char *function_name, const char *message_format, ...)
{
	char *last_error = (char *)last_error_msg_get();
	va_list arg;

	va_start(arg, message_format);
	core_log_va(last_error, CORE_LAST_ERROR_MSG_MAXPRINT,
		CORE_LOG_LEVEL_ERROR, use_errno, file_name, line_no,
		function_name, message_format, arg);
	va_end(arg);
}

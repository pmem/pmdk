// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020-2024, Intel Corporation */

/*
 * log.c -- support for logging output to either syslog or stderr or
 * via user defined function
 */

/*
 * Note: The undef below is critical to use the XSI-compliant version of
 * the strerror_r(3) instead of the GNU-specific. Otherwise, the produced
 * error string may not end up in the log buffer.
 */
#undef _GNU_SOURCE
#include <string.h>

#include <stdarg.h>
#include <syslog.h>
#include <time.h>
#include <errno.h>
#ifdef ATOMIC_OPERATIONS_SUPPORTED
#include <stdatomic.h>
#endif /* ATOMIC_OPERATIONS_SUPPORTED */
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
#define CORE_LOG_THRESHOLD_AUX_DEFAULT CORE_LOG_LEVEL_HARK
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

static void
core_log_lib_info(void)
{
	CORE_LOG_HARK("src version: " SRCVERSION);
#if SDS_ENABLED
	CORE_LOG_HARK("compiled with support for shutdown state");
#endif
#if NDCTL_ENABLED
	CORE_LOG_HARK("compiled with libndctl 63+");
#endif
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
			context_old, context)) {
		core_log_lib_info();
		return 0;
	}

	(void) __sync_bool_compare_and_swap(&Core_log_function,
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

	if (level < CORE_LOG_LEVEL_HARK || level > CORE_LOG_LEVEL_DEBUG)
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

static void inline
core_log_va(char *buf, size_t buf_len, enum core_log_level level,
	int errnum, const char *file_name, int line_no,
	const char *function_name, const char *message_format, va_list arg)
{
	int msg_len = vsnprintf(buf, buf_len, message_format, arg);
	if (msg_len < 0)
		goto end;

	if ((size_t)msg_len < buf_len - 1 && errnum != NO_ERRNO) {
		/*
		 * Ask for the error string right after the already printed
		 * message.
		 */
		char *msg_ptr = buf + msg_len;
		size_t buf_len_left = buf_len - (size_t)msg_len;
		/*
		 * If it fails, the best thing to do is to at least pass
		 * the log message as is.
		 */
		(void) strerror_r(errnum, msg_ptr, buf_len_left);
	}

	/*
	 * Despite this check is already done when the function is called from
	 * the CORE_LOG() macro it has to be done here again since it is not
	 * performed in the case of the CORE_LOG_TO_LAST macro. Sorry.
	 */
	if (level > Core_log_threshold[CORE_LOG_THRESHOLD])
		goto end;

	if (0 == Core_log_function)
		goto end;

	((core_log_function *)Core_log_function)(Core_log_function_context,
		level, file_name, line_no, function_name, buf);

end:
	if (errnum != NO_ERRNO)
		errno = errnum;
}

void
core_log(enum core_log_level level, int errnum, const char *file_name,
	int line_no, const char *function_name, const char *message_format, ...)
{
	char message[_CORE_LOG_MSG_MAXPRINT] = "";
	char *buf = message;
	size_t buf_len = sizeof(message);
	if (level == CORE_LOG_LEVEL_ERROR_LAST) {
		level = CORE_LOG_LEVEL_ERROR;
		buf = (char *)last_error_msg_get();
		buf_len = CORE_LAST_ERROR_MSG_MAXPRINT;
	}

	va_list arg;
	va_start(arg, message_format);
	core_log_va(buf, buf_len, level, errnum, file_name, line_no,
		function_name, message_format, arg);
	va_end(arg);
}

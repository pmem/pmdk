/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2020-2024, Intel Corporation */

/*
 * log_internal.h -- internal logging interfaces
 */

#ifndef CORE_LOG_INTERNAL_H
#define CORE_LOG_INTERNAL_H

#include <stdlib.h>
#include <stdint.h>
#include <errno.h>

#ifdef ATOMIC_OPERATIONS_SUPPORTED
#include <stdatomic.h>
#endif /* ATOMIC_OPERATIONS_SUPPORTED */

/* Required to work properly with pmembench. */
#ifdef __cplusplus
extern "C" {
#endif

enum core_log_level {
	/* only basic library info */
	CORE_LOG_LEVEL_HARK,
	/* an error that causes the library to stop working immediately */
	CORE_LOG_LEVEL_FATAL,
	/* an error that causes the library to stop working properly */
	CORE_LOG_LEVEL_ERROR,
	/* an errors that could be handled in the upper level */
	CORE_LOG_LEVEL_WARNING,
	/* non-massive info mainly related to public API function completions */
	CORE_LOG_LEVEL_NOTICE,
	/* massive info e.g. every write operation indication */
	CORE_LOG_LEVEL_INFO,
	/* debug info e.g. write operation dump */
	CORE_LOG_LEVEL_DEBUG,
	CORE_LOG_LEVEL_MAX
};

#define CORE_LOG_LEVEL_ERROR_LAST ((enum core_log_level) \
	(CORE_LOG_LEVEL_ERROR + CORE_LOG_LEVEL_MAX))

enum core_log_threshold {
	/*
	 * the main threshold level - the logging messages above this level
	 * won't trigger the logging functions
	 */
	CORE_LOG_THRESHOLD,
	/*
	 * the auxiliary threshold level - may or may not be used by the logging
	 * function
	 */
	CORE_LOG_THRESHOLD_AUX,
	CORE_LOG_THRESHOLD_MAX
};

int core_log_set_threshold(enum core_log_threshold threshold,
	enum core_log_level level);

int core_log_get_threshold(enum core_log_threshold threshold,
	enum core_log_level *level);

volatile enum core_log_level _core_log_get_threshold_internal(void);

/*
 * the type used for defining logging functions
 */
typedef void core_log_function(
	/* the log level of the message */
	enum core_log_level level,
	/* name of the source file where the message coming from */
	const char *file_name,
	/* the source file line where the message coming from */
	unsigned line_no,
	/* the function name where the message coming from */
	const char *function_name,
	/* message */
	const char *message);

#define CORE_LOG_USE_DEFAULT_FUNCTION (NULL)

int core_log_set_function(core_log_function *log_function);

void core_log_init(void);

void core_log_fini(void);

/*
 * The actual maximum expected log line is 407.
 * An additional byte is used to detect buffer overflow in core_log tests.
 * If the actual max message length is equal to 408, it means we have a buffer
 * overflow.
 */
#define _CORE_LOG_MSG_MAXPRINT 408 /* maximum expected log line + 1 */

static inline int
core_log_error_translate(int ret)
{
	if (ret != 0) {
		errno = ret;
		return 1;
	}

	return 0;
}

#define NO_ERRNO (-1)

void core_log(enum core_log_level level, int errnum, const char *file_name,
	unsigned line_no, const char *function_name, const char *message_format,
	...);

#define _CORE_LOG(level, errnum, format, ...) \
	do { \
		if (level <= \
				_core_log_get_threshold_internal()) { \
			core_log(level, errnum, __FILE__, __LINE__, \
			__func__, format, ##__VA_ARGS__); \
		} \
	} while (0)

/*
 * Can't check the logging level here when logging to the last error message.
 * Since the log message has to be generated anyway.
 */
#define CORE_LOG_TO_LAST(errnum, format, ...) \
	core_log(CORE_LOG_LEVEL_ERROR_LAST, errnum, __FILE__, __LINE__, \
		__func__, format, ##__VA_ARGS__)

/* The value fine-tuned to accommodate all possible errno message strings. */
#define _CORE_LOG_MAX_ERRNO_MSG 50

#define _CORE_LOG_W_ERRNO(level, format, ...) \
		_CORE_LOG(level, errno, format ": ", ##__VA_ARGS__)

/*
 * Set of macros that should be used as the primary API for logging.
 * Direct call to log shall be used only in exceptional, corner cases.
 */
#define CORE_LOG_DEBUG(format, ...) \
	_CORE_LOG(CORE_LOG_LEVEL_DEBUG, NO_ERRNO, format, ##__VA_ARGS__)

#define CORE_LOG_INFO(format, ...) \
	_CORE_LOG(CORE_LOG_LEVEL_INFO, NO_ERRNO, format, ##__VA_ARGS__)

#define CORE_LOG_NOTICE(format, ...) \
	_CORE_LOG(CORE_LOG_LEVEL_NOTICE, NO_ERRNO, format, ##__VA_ARGS__)

#define CORE_LOG_WARNING(format, ...) \
	_CORE_LOG(CORE_LOG_LEVEL_WARNING, NO_ERRNO, format, ##__VA_ARGS__)

#define CORE_LOG_ERROR(format, ...) \
	_CORE_LOG(CORE_LOG_LEVEL_ERROR, NO_ERRNO, format, ##__VA_ARGS__)

#define CORE_LOG_FATAL(format, ...) \
	do { \
		_CORE_LOG(CORE_LOG_LEVEL_FATAL, NO_ERRNO, \
			format, ##__VA_ARGS__); \
		abort(); \
	} while (0)

#define CORE_LOG_HARK(format, ...) \
	_CORE_LOG(CORE_LOG_LEVEL_HARK, NO_ERRNO, format, ##__VA_ARGS__)

/*
 * 'With errno' macros' flavours. Append string describing the current errno
 * value.
 */

#define CORE_LOG_WARNING_W_ERRNO(format, ...) \
	_CORE_LOG_W_ERRNO(CORE_LOG_LEVEL_WARNING, format, ##__VA_ARGS__)

#define CORE_LOG_ERROR_W_ERRNO(format, ...) \
	_CORE_LOG_W_ERRNO(CORE_LOG_LEVEL_ERROR, format, ##__VA_ARGS__)

#define CORE_LOG_FATAL_W_ERRNO(format, ...) \
	do { \
		_CORE_LOG_W_ERRNO(CORE_LOG_LEVEL_FATAL, \
			format, ##__VA_ARGS__); \
		abort(); \
	} while (0)

/*
 * 'Last' macros' flavours. Additionally writes the produced error message
 * to the last error message's TLS buffer making it available to the end user
 * via the *_errormsg() API calls.
 */

#define CORE_LOG_ERROR_LAST(format, ...) \
	CORE_LOG_TO_LAST(NO_ERRNO, format, ##__VA_ARGS__)

#define CORE_LOG_ERROR_W_ERRNO_LAST(format, ...) \
		CORE_LOG_TO_LAST(errno, format ": ", ##__VA_ARGS__);

/* Aliases */

#define ERR_W_ERRNO(f, ...)\
	CORE_LOG_ERROR_W_ERRNO_LAST(f, ##__VA_ARGS__)

#define ERR_WO_ERRNO(f, ...)\
	CORE_LOG_ERROR_LAST(f, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif /* CORE_LOG_INTERNAL_H */

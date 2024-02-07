/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2020-2024, Intel Corporation */

/*
 * log_internal.h -- internal logging interfaces
 */

#ifndef CORE_LOG_INTERNAL_H
#define CORE_LOG_INTERNAL_H

#include <stdlib.h>
#include <string.h>
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
	/* all messages will be suppressed */
	CORE_LOG_DISABLED = -1,
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
};

/* Not meant to be used directly. */
#define CORE_LOG_LEVEL_ALWAYS (CORE_LOG_DISABLED - 1)

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

/*
 * the type used for defining logging functions
 */
typedef void core_log_function(
	/* the context provided when setting the log function */
	void *context,
	/* the log level of the message */
	enum core_log_level level,
	/* name of the source file where the message coming from */
	const char *file_name,
	/* the source file line where the message coming from */
	const int line_no,
	/* the function name where the message coming from */
	const char *function_name,
	/* message */
	const char *message);

#define CORE_LOG_USE_DEFAULT_FUNCTION (NULL)

int core_log_set_function(core_log_function *log_function, void *context);

/* pointer to the logging function */
extern
#ifdef ATOMIC_OPERATIONS_SUPPORTED
_Atomic
#endif /* ATOMIC_OPERATIONS_SUPPORTED */
uintptr_t Core_log_function;

/* the logging function's context */
extern
#ifdef ATOMIC_OPERATIONS_SUPPORTED
_Atomic
#endif /* ATOMIC_OPERATIONS_SUPPORTED */
void *Core_log_function_context;

/* threshold levels */
extern
#ifdef ATOMIC_OPERATIONS_SUPPORTED
_Atomic
#endif /* ATOMIC_OPERATIONS_SUPPORTED */
enum core_log_level Core_log_threshold[CORE_LOG_THRESHOLD_MAX];

void core_log_init(void);

void core_log_fini(void);

static inline int
core_log_error_translate(int ret)
{
	if (ret != 0) {
		errno = ret;
		return 1;
	}

	return 0;
}

void core_log(enum core_log_level level, const char *file_name, int line_no,
	const char *function_name, const char *message_format, ...);

/* Only error messages can last. So, no level has to be specified. */
void core_log_to_last(const char *file_name, int line_no,
	const char *function_name, const char *message_format, ...);

#define CORE_LOG(level, format, ...) \
	do { \
		if (level <= Core_log_threshold[CORE_LOG_THRESHOLD]) { \
			core_log(level, __FILE__, __LINE__, __func__, \
				format, ##__VA_ARGS__); \
		} \
	} while (0)

/*
 * Can't check the logging level here when logging to the last error message.
 * Since the log message has to be generated anyway.
 */
#define CORE_LOG_TO_LAST(format, ...) \
	core_log_to_last(__FILE__, __LINE__, __func__, format, ##__VA_ARGS__)

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

#define CORE_LOG_MAX_ERR_MSG 128

/*
 * Set of macros that should be used as the primary API for logging.
 * Direct call to log shall be used only in exceptional, corner cases.
 */
#define CORE_LOG_DEBUG(format, ...) \
	CORE_LOG(CORE_LOG_LEVEL_DEBUG, format, ##__VA_ARGS__)

#define CORE_LOG_INFO(format, ...) \
	CORE_LOG(CORE_LOG_LEVEL_INFO, format, ##__VA_ARGS__)

#define CORE_LOG_NOTICE(format, ...) \
	CORE_LOG(CORE_LOG_LEVEL_NOTICE, format, ##__VA_ARGS__)

#define CORE_LOG_WARNING(format, ...) \
	CORE_LOG(CORE_LOG_LEVEL_WARNING, format, ##__VA_ARGS__)

#define CORE_LOG_ERROR(format, ...) \
	CORE_LOG(CORE_LOG_LEVEL_ERROR, format, ##__VA_ARGS__)

#define CORE_LOG_FATAL(format, ...) \
	do { \
		CORE_LOG(CORE_LOG_LEVEL_FATAL, format, ##__VA_ARGS__); \
		abort(); \
	} while (0)

#define CORE_LOG_ALWAYS(format, ...) \
	CORE_LOG(CORE_LOG_LEVEL_ALWAYS, format, ##__VA_ARGS__)

/*
 * 'With errno' macros' flavours. Append string describing the current errno
 * value.
 */

#define CORE_LOG_WARNING_W_ERRNO(format, ...) \
	do { \
		char buf[CORE_LOG_MAX_ERR_MSG]; \
		char *error_str; \
		_CORE_LOG_STRERROR(buf, CORE_LOG_MAX_ERR_MSG, &error_str); \
		CORE_LOG(CORE_LOG_LEVEL_WARNING, format ": %s", ##__VA_ARGS__, \
			error_str); \
	} while (0)

#define CORE_LOG_ERROR_W_ERRNO(format, ...) \
	do { \
		char buf[CORE_LOG_MAX_ERR_MSG]; \
		char *error_str; \
		_CORE_LOG_STRERROR(buf, CORE_LOG_MAX_ERR_MSG, &error_str); \
		CORE_LOG(CORE_LOG_LEVEL_ERROR, format ": %s", ##__VA_ARGS__, \
			error_str); \
	} while (0)

#define CORE_LOG_FATAL_W_ERRNO(format, ...) \
	do { \
		char buf[CORE_LOG_MAX_ERR_MSG]; \
		char *error_str; \
		_CORE_LOG_STRERROR(buf, CORE_LOG_MAX_ERR_MSG, &error_str); \
		CORE_LOG(CORE_LOG_LEVEL_FATAL, format ": %s", ##__VA_ARGS__, \
			error_str); \
		abort(); \
	} while (0)

/*
 * 'Last' macros' flavours. Additionally writes the produced error message
 * to the last error message's TLS buffer making it available to the end user
 * via the *_errormsg() API calls.
 */

#define CORE_LOG_ERROR_LAST(format, ...) \
	CORE_LOG_TO_LAST(format, ##__VA_ARGS__)

#define CORE_LOG_ERROR_W_ERRNO_LAST(format, ...) \
	do { \
		char buf[CORE_LOG_MAX_ERR_MSG]; \
		char *error_str; \
		_CORE_LOG_STRERROR(buf, CORE_LOG_MAX_ERR_MSG, &error_str); \
		CORE_LOG_TO_LAST(format ": %s", ##__VA_ARGS__, error_str); \
	} while (0)

#ifdef __cplusplus
}
#endif

#endif /* CORE_LOG_INTERNAL_H */

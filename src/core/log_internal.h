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
	/* printf(3)-like format string of the message */
	const char *message_format,
	/* additional arguments of the message format string */
	...);

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

#define CORE_LOG(level, format, ...) \
	do { \
		if (level <= Core_log_threshold[CORE_LOG_THRESHOLD] && \
				0 != Core_log_function) { \
			((core_log_function *)Core_log_function)( \
				Core_log_function_context, level, __FILE__, \
				__LINE__, __func__, format, ##__VA_ARGS__); \
		} \
	} while (0)

#define CORE_LOG_LEVEL_ALWAYS (CORE_LOG_DISABLED - 1)

void core_log_default_function(void *context, enum core_log_level level,
	const char *file_name, const int line_no, const char *function_name,
	const char *message_format, ...);

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

#define CORE_LOG_MAX_ERR_MSG 128
#define CORE_LOG_FATAL_W_ERRNO(format, ...) \
	do { \
		char buff[CORE_LOG_MAX_ERR_MSG]; \
		CORE_LOG(CORE_LOG_LEVEL_FATAL, \
			format ": %s", ##__VA_ARGS__, \
			strerror_r(errno, buff, \
				CORE_LOG_MAX_ERR_MSG)); \
		abort(); \
	} while (0)

#define CORE_LOG_ALWAYS(format, ...) \
	CORE_LOG(CORE_LOG_LEVEL_ALWAYS, format, ##__VA_ARGS__)

/*
 * Replacement for ERR("!*") macro (w/ errno).
 * 'f' stands here for 'function' or 'format' where the latter may accept
 * additional arguments.
 */
#define CORE_LOG_ERROR_WITH_ERRNO(f, ...) \
	CORE_LOG_ERROR(f ": %s", ##__VA_ARGS__, strerror(errno))

static inline int
core_log_error_translate(int ret)
{
	if (ret != 0) {
		errno = ret;
		return 1;
	}

	return 0;
}

#endif /* CORE_LOG_INTERNAL_H */

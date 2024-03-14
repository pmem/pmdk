/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2024, Intel Corporation */

/*
 * libpmemobj/log.h -- the controls for the libpmemobj's logging output
 */

#ifndef LIBPMEMOBJ_LOG_H
#define LIBPMEMOBJ_LOG_H 1

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Available log levels. Log levels are used in the logging API calls
 * to indicate logging message severity. Log levels are also used
 * to define thresholds for the logging.
 */
enum pmemobj_log_level {
	/* only basic library info */
	PMEMOBJ_LOG_LEVEL_HARK,
	/* an error that causes the library to stop working immediately */
	PMEMOBJ_LOG_LEVEL_FATAL,
	/* an error that causes the library to stop working properly */
	PMEMOBJ_LOG_LEVEL_ERROR,
	/* an errors that could be handled in the upper level */
	PMEMOBJ_LOG_LEVEL_WARNING,
	/* non-massive info mainly related to public API function completions */
	PMEMOBJ_LOG_LEVEL_NOTICE,
	/* massive info e.g. every write operation indication */
	PMEMOBJ_LOG_LEVEL_INFO,
	/* debug info e.g. write operation dump */
	PMEMOBJ_LOG_LEVEL_DEBUG,
};

enum pmemobj_log_threshold {
	/*
	 * the main threshold level - the logging messages above this level
	 * won't trigger the logging functions
	 */
	PMEMOBJ_LOG_THRESHOLD,
	/*
	 * the auxiliary threshold level - may or may not be used by the logging
	 * function
	 */
	PMEMOBJ_LOG_THRESHOLD_AUX,
	PMEMOBJ_LOG_THRESHOLD_MAX
};

/*
 * pmemobj_log_set_threshold - set the logging threshold value
 */
int pmemobj_log_set_threshold(enum pmemobj_log_threshold threshold,
	enum pmemobj_log_level value);

/*
 * pmemobj_log_get_threshold - get the logging threshold value
 */
int pmemobj_log_get_threshold(enum pmemobj_log_threshold threshold,
	enum pmemobj_log_level *value);

/*
 * the type used for defining logging functions
 */
typedef void pmemobj_log_function(
	/* the log level of the message */
	enum pmemobj_log_level level,
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

#define PMEMOBJ_LOG_USE_DEFAULT_FUNCTION (NULL)

/*
 * pmemobj_log_set_function - set the logging function
 */
int pmemobj_log_set_function(pmemobj_log_function *log_function);

#ifdef __cplusplus
}
#endif

#endif	/* libpmemobj/log.h */

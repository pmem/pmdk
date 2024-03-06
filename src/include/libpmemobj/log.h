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
 * pmemobj_log_set_threshold - set the logging threshold level
 *
 * SYNOPSIS
 *
 *	int pmemobj_log_set_threshold(enum pmemobj_log_threshold threshold,
 *              enum pmemobj_log_level level);
 *
 *	enum log_level {
 *		PMEMOBJ_LOG_LEVEL_HARK,
 *		PMEMOBJ_LOG_LEVEL_FATAL,
 *		PMEMOBJ_LOG_LEVEL_ERROR,
 *		PMEMOBJ_LOG_LEVEL_WARNING,
 *		PMEMOBJ_LOG_LEVEL_NOTICE,
 *		PMEMOBJ_LOG_LEVEL_INFO,
 *		PMEMOBJ_LOG_LEVEL_DEBUG,
 *	};
 *
 *	enum log_threshold {
 *		PMEMOBJ_LOG_THRESHOLD,
 *		PMEMOBJ_LOG_THRESHOLD_AUX,
 *		PMEMOBJ_LOG_THRESHOLD_MAX
 *	};
 *
 * DESCRIPTION
 * pmemobj_log_set_threshold() sets the logging threshold level.
 *
 * Available thresholds are:
 * - PMEMOBJ_LOG_THRESHOLD - the main threshold used to filter out undesired
 *   logging messages. Messages on a higher level than the primary threshold
 *   level are ignored. PMEMOBJ_LOG_LEVEL_HARK shall be used to suppress
 *   logging.
 *   The default value is PMEMOBJ_LOG_WARNING.
 * - PMEMOBJ_LOG_THRESHOLD_AUX - the auxiliary threshold intended for use inside
 *   the logging function (please see log_get_threshold(3)). The logging
 *   function may or may not take this threshold into consideration. The default
 *   value is PMEMOBJ_LOG_LEVEL_HARK.
 *
 * Available threshold levels are defined by enum log_level:
 * - PMEMOBJ_LOG_LEVEL_HARK - only basic library info
 * - PMEMOBJ_LOG_LEVEL_FATAL - an error that causes the library to stop working
 *   immediately
 * - PMEMOBJ_LOG_LEVEL_ERROR - an error that causes the library to stop working
 *   properly
 * - PMEMOBJ_LOG_LEVEL_WARNING - an error that could be handled in the upper
 *   level
 * - PMEMOBJ_LOG_LEVEL_NOTICE - non-massive info mainly related to public API
 *   function completions
 * - PMEMOBJ_LOG_LEVEL_INFO - massive info e.g. every write operation indication
 * - PMEMOBJ_LOG_LEVEL_DEBUG - debug info e.g. write operation dump
 *
 * THE DEFAULT LOGGING FUNCTION
 * The default logging function writes messages to syslog(3) and to stderr(3),
 * where syslog(3) is the primary destination (PMEMOBJ_LOG_THRESHOLD applies)
 * whereas stderr(3) is the secondary destination (PMEMOBJ_LOG_THRESHOLD_AUX
 * applies).
 *
 * RETURN VALUE
 * pmemobj_log_set_threshold() function returns 0 on success or returns
 * a non-zero value and sets errno on failure.
 *
 * ERRORS
 * pmemobj_log_set_threshold() can set the following errno values on fail:
 * - EINVAL - threshold is not PMEMOBJ_LOG_THRESHOLD nor
 *   PMEMOBJ_LOG_THRESHOLD_AUX
 * - EINVAL - level is not a value defined by enum log_level type
 * - EAGAIN - a temporary error occurred, the retry may fix the problem
 *
 * SEE ALSO
 * pmemobj_log_get_threshold(3), pmemobj_log_set_function(3).
 */
int pmemobj_log_set_threshold(enum pmemobj_log_threshold threshold,
	enum pmemobj_log_level level);

/*
 * pmemobj_log_get_threshold - get the logging threshold level
 *
 * SYNOPSIS
 *
 *	int pmemobj_log_get_threshold(enum pmemobj_log_threshold threshold,
 *              enum pmemobj_log_level *level);
 *
 * DESCRIPTION
 * pmemobj_log_get_threshold() gets the current level of the threshold.
 * See pmemobj_log_set_threshold(3) for available thresholds and levels.
 *
 * RETURN VALUE
 * pmemobj_log_get_threshold() function returns 0 on success or returns
 * a non-zero value and sets errno on failure.
 *
 * ERRORS
 * pmemobj_log_get_threshold() can fail with the following errors:
 * - EINVAL - threshold is not CORE_LOG_THRESHOLD nor CORE_LOG_THRESHOLD_AUX
 * - EINVAL - *level is NULL
 *
 * SEE ALSO
 * pmemobj_log_set_function(3), pmemobj_log_set_threshold(3).
 */
int pmemobj_log_get_threshold(enum pmemobj_log_threshold threshold,
	enum pmemobj_log_level *level);

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
 *
 * SYNOPSIS
 *
 *	typedef void pmemobj_log_function(
 *		enum pmemobj_log_level level,
 *		const char *file_name,
 *		const int line_no,
 *		const char *function_name,
 *		const char *message_format,
 *		...);
 *
 *	int pmemobj_log_set_function(pmemobj_log_function *log_function);
 *
 * DESCRIPTION
 * pmemobj_log_set_function() allows choosing the function which will get all
 * the generated logging messages. The log_function can be either
 * PMEMOBJ_LOG_USE_DEFAULT_FUNCTION which will use the default logging function
 * (built into the library) or a pointer to a user-defined function.
 *
 * Parameters of a user-defined log function are as follow:
 * - level - the log level of the message
 * - file_name - name of the source file where the message coming from. It could
 *               be set to NULL and
 *   in such case neither line_no nor function_name are provided.
 * - line_no - the source file line where the message coming from
 * - function_name - the function name where the message coming from
 * - message_format - printf(3)-like format string of the message
 * - "..." - additional arguments of the message format string
 *
 * THE DEFAULT LOGGING FUNCTION
 * The initial value of the logging function is CORE_LOG_USE_DEFAULT_FUNCTION.
 * This function writes messages to syslog(3) (the primary destination) and to
 * stderr(3) (the secondary destination).
 *
 * RETURN VALUE
 * pmemobj_log_set_function() function returns 0 on success or returns
 * a non-zero value and sets errno on failure.
 *
 * ERRORS
 * - EAGAIN - a temporary error occurred, the retry may fix the problem
 *
 * NOTE
 * The logging messages on the levels above the CORE_LOG_THRESHOLD level won't
 * trigger the logging function.
 *
 * The user defined function must be thread-safe.
 *
 * SEE ALSO
 * pmemobj_log_get_threshold(3), pmemobj_log_set_threshold(3).
 */
int pmemobj_log_set_function(pmemobj_log_function *log_function);

#ifdef __cplusplus
}
#endif

#endif	/* libpmemobj/log.h */

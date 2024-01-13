/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2019-2022, Intel Corporation */
/* Copyright 2021-2022, Fujitsu */

/*
 * librpma.h -- definitions of librpma entry points
 *
 * This library provides low-level support for remote access to persistent memory utilizing
 * RDMA-capable NICs.
 */

#ifndef LIBRPMA_H
#define LIBRPMA_H 1

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RPMA_E_INVAL		(-100004) /* Invalid argument */
#define RPMA_E_AGAIN		(-100007) /* Temporary error */

/* librpma logging mechanism control */

/*
 * Available log levels in librpma. Log levels (except RPMA_LOG_DISABLED) are used in logging API
 * calls to indicate logging message severity. Log levels are also used to define thresholds for
 * logging.
 */
enum rpma_log_level {
	/* all messages will be suppressed */
	RPMA_LOG_DISABLED = -1,
	/* an error that causes the library to stop working immediately */
	RPMA_LOG_LEVEL_FATAL,
	/* an error that causes the library to stop working properly */
	RPMA_LOG_LEVEL_ERROR,
	/* an errors that could be handled in the upper level */
	RPMA_LOG_LEVEL_WARNING,
	/*
	 * non-massive info mainly related to public API function completions e.g. connection
	 * established
	 */
	RPMA_LOG_LEVEL_NOTICE,
	/* massive info e.g. every write operation indication */
	RPMA_LOG_LEVEL_INFO,
	/* debug info e.g. write operation dump */
	RPMA_LOG_LEVEL_DEBUG,
};

enum rpma_log_threshold {
	/*
	 * the main threshold level - the logging messages above this level won't trigger
	 * the logging functions
	 */
	RPMA_LOG_THRESHOLD,
	/*
	 * the auxiliary threshold level - may or may not be used by the logging function
	 */
	RPMA_LOG_THRESHOLD_AUX,
	RPMA_LOG_THRESHOLD_MAX
};

/** 3
 * rpma_log_set_threshold - set the logging threshold level
 *
 * SYNOPSIS
 *
 *	#include <librpma.h>
 *
 *	int rpma_log_set_threshold(enum rpma_log_threshold threshold, enum rpma_log_level level);
 *
 *	enum rpma_log_level {
 *		RPMA_LOG_DISABLED,
 *		RPMA_LOG_LEVEL_FATAL,
 *		RPMA_LOG_LEVEL_ERROR,
 *		RPMA_LOG_LEVEL_WARNING,
 *		RPMA_LOG_LEVEL_NOTICE,
 *		RPMA_LOG_LEVEL_INFO,
 *		RPMA_LOG_LEVEL_DEBUG,
 *	};
 *
 *	enum rpma_log_threshold {
 *		RPMA_LOG_THRESHOLD,
 *		RPMA_LOG_THRESHOLD_AUX,
 *		RPMA_LOG_THRESHOLD_MAX
 *	};
 *
 * DESCRIPTION
 * rpma_log_set_threshold() sets the logging threshold level.
 *
 * Available thresholds are:
 * - RPMA_LOG_THRESHOLD - the main threshold used to filter out undesired logging messages.
 *   Messages on a higher level than the primary threshold level are ignored. RPMA_LOG_DISABLED
 *   shall be used to suppress logging. The default value is RPMA_LOG_WARNING.
 * - RPMA_LOG_THRESHOLD_AUX - the auxiliary threshold intended for use inside the logging function
 *   (please see rpma_log_get_threshold(3)). The logging function may or may not take this
 *   threshold into consideration. The default value is RPMA_LOG_DISABLED.
 *
 * Available threshold levels are defined by enum rpma_log_level:
 * - RPMA_LOG_DISABLED - all messages will be suppressed
 * - RPMA_LOG_LEVEL_FATAL - an error that causes the library to stop working immediately
 * - RPMA_LOG_LEVEL_ERROR - an error that causes the library to stop working properly
 * - RPMA_LOG_LEVEL_WARNING - an error that could be handled in the upper level
 * - RPMA_LOG_LEVEL_NOTICE - non-massive info mainly related to public API function completions
 *   e.g. connection established
 * - RPMA_LOG_LEVEL_INFO - massive info e.g. every write operation indication
 * - RPMA_LOG_LEVEL_DEBUG - debug info e.g. write operation dump
 *
 * THE DEFAULT LOGGING FUNCTION
 * The default logging function writes messages to syslog(3) and to stderr(3), where syslog(3) is
 * the primary destination (RPMA_LOG_THRESHOLD applies) whereas stderr(3) is the secondary
 * destination (RPMA_LOG_THRESHOLD_AUX applies).
 *
 * RETURN VALUE
 * rpma_log_syslog_set_threshold() function returns 0 on success or a negative error code on
 * failure.
 *
 * ERRORS
 * rpma_log_set_threshold() can fail with the following errors:
 * - RPMA_E_INVAL - threshold is not RPMA_LOG_THRESHOLD nor RPMA_LOG_THRESHOLD_AUX
 * - RPMA_E_INVAL - level is not a value defined by enum rpma_log_level type
 * - RPMA_E_AGAIN - a temporary error occurred, the retry may fix the problem
 *
 * SEE ALSO
 * rpma_log_get_threshold(3), rpma_log_set_function(3), librpma(7) and https://pmem.io/rpma/
 */
int rpma_log_set_threshold(enum rpma_log_threshold threshold, enum rpma_log_level level);

/** 3
 * rpma_log_get_threshold - get the logging threshold level
 *
 * SYNOPSIS
 *
 *	#include <librpma.h>
 *
 *	int rpma_log_get_threshold(enum rpma_log_threshold threshold, enum rpma_log_level *level);
 *
 * DESCRIPTION
 * rpma_log_get_threshold() gets the current level of the threshold.
 * See rpma_log_set_threshold(3) for available thresholds and levels.
 *
 * RETURN VALUE
 * rpma_log_get_threshold() function returns 0 on success or a negative error code on failure.
 *
 * ERRORS
 * rpma_log_get_threshold() can fail with the following errors:
 * - RPMA_E_INVAL - threshold is not RPMA_LOG_THRESHOLD nor RPMA_LOG_THRESHOLD_AUX
 * - RPMA_E_INVAL - *level is NULL
 *
 * SEE ALSO
 * rpma_log_set_function(3), rpma_log_set_threshold(3), librpma(7) and https://pmem.io/rpma/
 */
int rpma_log_get_threshold(enum rpma_log_threshold threshold, enum rpma_log_level *level);

/*
 * the type used for defining logging functions
 */
typedef void rpma_log_function(
	/* the log level of the message */
	enum rpma_log_level level,
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

#define RPMA_LOG_USE_DEFAULT_FUNCTION (NULL)

/** 3
 * rpma_log_set_function - set the logging function
 *
 * SYNOPSIS
 *
 *	#include <librpma.h>
 *
 *	typedef void rpma_log_function(
 *		enum rpma_log_level level,
 *		const char *file_name,
 *		const int line_no,
 *		const char *function_name,
 *		const char *message_format,
 *		...);
 *
 *	int rpma_log_set_function(rpma_log_function *log_function);
 *
 * DESCRIPTION
 * rpma_log_set_function() allows choosing the function which will get all the generated logging
 * messages. The log_function can be either RPMA_LOG_USE_DEFAULT_FUNCTION which will use
 * the default logging function (built into the library) or a pointer to a user-defined function.
 *
 * Parameters of a user-defined log function are as follow:
 * - level - the log level of the message
 * - file_name - name of the source file where the message coming from. It could be set to NULL and
 *   in such case neither line_no nor function_name are provided.
 * - line_no - the source file line where the message coming from
 * - function_name - the function name where the message coming from
 * - message_format - printf(3)-like format string of the message
 * - "..." - additional arguments of the message format string
 *
 * THE DEFAULT LOGGING FUNCTION
 * The initial value of the logging function is RPMA_LOG_USE_DEFAULT_FUNCTION. This function writes
 * messages to syslog(3) (the primary destination) and to stderr(3) (the secondary destination).
 *
 * RETURN VALUE
 * rpma_log_set_function() function returns 0 on success or error code on failure.
 *
 * ERRORS
 * - RPMA_E_AGAIN - a temporary error occurred, the retry may fix the problem
 *
 * NOTE
 * The logging messages on the levels above the RPMA_LOG_THRESHOLD level won't trigger the logging
 * function.
 *
 * The user defined function must be thread-safe.
 *
 * SEE ALSO
 * rpma_log_get_threshold(3), rpma_log_set_threshold(3), librpma(7) and https://pmem.io/rpma/
 */
int rpma_log_set_function(rpma_log_function *log_function);

void rpma_log_init(void);

void rpma_log_fini(void);

#ifdef __cplusplus
}
#endif

#endif /* LIBRPMA_H */

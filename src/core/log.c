// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020-2022, Intel Corporation */

/*
 * log.c -- support for logging output to either syslog or stderr or
 * via user defined function
 */

#include <stdarg.h>
#include <syslog.h>
#include <time.h>
#ifdef ATOMIC_OPERATIONS_SUPPORTED
#include <stdatomic.h>
#endif /* ATOMIC_OPERATIONS_SUPPORTED */
#include <string.h>

#include "librpma.h"
#include "log_default.h"
#include "log_internal.h"

/*
 * Default levels of the logging thresholds
 */
#ifdef DEBUG
#define RPMA_LOG_THRESHOLD_DEFAULT RPMA_LOG_LEVEL_DEBUG
#define RPMA_LOG_THRESHOLD_AUX_DEFAULT RPMA_LOG_LEVEL_WARNING
#else
#define RPMA_LOG_THRESHOLD_DEFAULT RPMA_LOG_LEVEL_WARNING
#define RPMA_LOG_THRESHOLD_AUX_DEFAULT RPMA_LOG_DISABLED
#endif

/*
 * Rpma_log_function -- pointer to the logging function saved as uintptr_t to make it _Atomic,
 * because function pointers cannot be _Atomic. By default it is rpma_log_default_function(),
 * but could be a user-defined logging function provided via rpma_log_set_function().
 */
#ifdef ATOMIC_OPERATIONS_SUPPORTED
_Atomic
#endif /* ATOMIC_OPERATIONS_SUPPORTED */
uintptr_t Rpma_log_function;

/* threshold levels */
#ifdef ATOMIC_OPERATIONS_SUPPORTED
_Atomic
#endif /* ATOMIC_OPERATIONS_SUPPORTED */
enum rpma_log_level Rpma_log_threshold[] = {
		RPMA_LOG_THRESHOLD_DEFAULT,
		RPMA_LOG_THRESHOLD_AUX_DEFAULT
};

/*
 * rpma_log_init -- initialize and set the default logging function
 */
void
rpma_log_init()
{
	/* enable the default logging function */
	rpma_log_default_init();
	while (RPMA_E_AGAIN == rpma_log_set_function(
				RPMA_LOG_USE_DEFAULT_FUNCTION))
		;
}

/*
 * rpma_log_fini -- disable logging and cleanup the default logging function
 */
void
rpma_log_fini()
{
	/*
	 * NULL-ed function pointer turns off the logging. No matter if
	 * the previous value was the default logging function or a user
	 * logging function.
	 */
	Rpma_log_function = 0;

	/* cleanup the default logging function */
	rpma_log_default_fini();
}

/* public librpma log API */

#if defined(RPMA_UNIT_TESTS) && !defined(ATOMIC_OPERATIONS_SUPPORTED)
int mock__sync_bool_compare_and_swap__function(uintptr_t *ptr, uintptr_t oldval, uintptr_t newval);
#define __sync_bool_compare_and_swap \
	mock__sync_bool_compare_and_swap__function
#endif

/*
 * rpma_log_set_function -- set the log function pointer either to
 * a user-provided function pointer or to the default logging function.
 */
int
rpma_log_set_function(rpma_log_function *log_function)
{

	if (log_function == RPMA_LOG_USE_DEFAULT_FUNCTION)
		log_function = rpma_log_default_function;

#ifdef ATOMIC_OPERATIONS_SUPPORTED
	atomic_store_explicit(&Rpma_log_function, (uintptr_t)log_function, __ATOMIC_SEQ_CST);
	return 0;
#else
	uintptr_t log_function_old = Rpma_log_function;

	if (__sync_bool_compare_and_swap(&Rpma_log_function,
			log_function_old, (uintptr_t)log_function))
		return 0;
	else
		return RPMA_E_AGAIN;
#endif /* ATOMIC_OPERATIONS_SUPPORTED */
}

#if defined(RPMA_UNIT_TESTS) && !defined(ATOMIC_OPERATIONS_SUPPORTED)
#undef __sync_bool_compare_and_swap
int mock__sync_bool_compare_and_swap__threshold(enum rpma_log_level *ptr,
	enum rpma_log_level oldval, enum rpma_log_level newval);
#define __sync_bool_compare_and_swap \
	mock__sync_bool_compare_and_swap__threshold
#endif

/*
 * rpma_log_set_threshold -- set the log level threshold
 */
int
rpma_log_set_threshold(enum rpma_log_threshold threshold,
			enum rpma_log_level level)
{
	if (threshold != RPMA_LOG_THRESHOLD && threshold != RPMA_LOG_THRESHOLD_AUX)
		return RPMA_E_INVAL;

	if (level < RPMA_LOG_DISABLED || level > RPMA_LOG_LEVEL_DEBUG)
		return RPMA_E_INVAL;

#ifdef ATOMIC_OPERATIONS_SUPPORTED
	atomic_store_explicit(&Rpma_log_threshold[threshold], level, __ATOMIC_SEQ_CST);
	return 0;
#else
	enum rpma_log_level level_old;
	while (RPMA_E_AGAIN == rpma_log_get_threshold(threshold, &level_old))
		;

	if (__sync_bool_compare_and_swap(&Rpma_log_threshold[threshold], level_old, level))
		return 0;
	else
		return RPMA_E_AGAIN;
#endif /* ATOMIC_OPERATIONS_SUPPORTED */
}

#ifdef RPMA_UNIT_TESTS
#undef __sync_bool_compare_and_swap
#endif

/*
 * rpma_log_get_threshold -- get the log level threshold
 */
int
rpma_log_get_threshold(enum rpma_log_threshold threshold,
			enum rpma_log_level *level)
{
	if (threshold != RPMA_LOG_THRESHOLD && threshold != RPMA_LOG_THRESHOLD_AUX)
		return RPMA_E_INVAL;

	if (level == NULL)
		return RPMA_E_INVAL;

#ifdef ATOMIC_OPERATIONS_SUPPORTED
	*level = atomic_load_explicit(&Rpma_log_threshold[threshold], __ATOMIC_SEQ_CST);
#else
	*level = Rpma_log_threshold[threshold];
#endif /* ATOMIC_OPERATIONS_SUPPORTED */

	return 0;
}

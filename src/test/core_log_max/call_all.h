/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2024, Intel Corporation */

/*
 * call_all.h --
 */

#ifndef CALL_ALL_H
#define CALL_ALL_H

/*
 * In general, a path in Linux can be as long as PATH_MAX (4096). However,
 * because of the memory constraints, it is undesirable to allocate log buffers
 * just in case the system is tested to its limits. Hence, we assumed a rational
 * path length is 128 bytes.
 *
 * Note: If the assumption won't be met and no limitation on the path string
 * will be introduced by the string format, the log buffer might be in great
 * part consumed to store the path whereas the latter part of the log message
 * would be lost.
 */
#define PATH \
	"Ut/imperdiet/dictum/dui/in/posuere/augue/accumsan/ut/Cras/et/neque/id/elit/porta/malesuada/Class/aptent/taciti/sociosqu//litora"

/* Basic log APIs */
void call_all_CORE_LOG_WARNING(void);
void call_all_CORE_LOG_ERROR(void);
void call_all_CORE_LOG_FATAL(void);

/* Log APIs appending an error string */
void call_all_CORE_LOG_WARNING_W_ERRNO(int errnum);
void call_all_CORE_LOG_ERROR_W_ERRNO(int errnum);
void call_all_CORE_LOG_FATAL_W_ERRNO(int errnum);

/* Log APIs that also store the error message for later use via TLS */
void call_all_CORE_LOG_ERROR_LAST(void);
void call_all_CORE_LOG_ERROR_W_ERRNO_LAST(int errnum);
void call_all_ERR_WO_ERRNO(void);
void call_all_ERR_W_ERRNO(int errnum);

#endif /* CALL_ALL_H */

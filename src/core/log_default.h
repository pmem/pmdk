/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2020-2024, Intel Corporation */

/*
 * log_default.h -- the default logging function definitions
 */

#ifndef CORE_LOG_DEFAULT_H
#define CORE_LOG_DEFAULT_H

void core_log_default_function(enum core_log_level level, const char *file_name,
	unsigned line_no, const char *function_name, const char *message);

void core_log_default_init(void);

void core_log_default_fini(void);

#endif /* CORE_LOG_DEFAULT_H */

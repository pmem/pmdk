/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2020-2022, Intel Corporation */

/*
 * log_default.h -- the default logging function definitions
 */

#ifndef LIBRPMA_LOG_DEFAULT_H
#define LIBRPMA_LOG_DEFAULT_H

#include "librpma.h"

void rpma_log_default_function(enum rpma_log_level level, const char *file_name, const int line_no,
	const char *function_name, const char *message_format, ...);

void rpma_log_default_init(void);

void rpma_log_default_fini(void);

#endif /* LIBRPMA_LOG_DEFAULT_H */

/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2014-2024, Intel Corporation */

/*
 * error_msg.h -- definitions for the "error_msg" module
 */

#ifndef CORE_ERROR_MSG_H
#define CORE_ERROR_MSG_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef NO_LIBPTHREAD
#define CORE_ERROR_MSG_MAXPRINT 8192 /* maximum expected log line */
#else
#define CORE_ERROR_MSG_MAXPRINT 256 /* maximum expected log line for libpmem */
#endif

void error_msg_init(void);
void error_msg_fini(void);

const char *error_msg_get(void);

#ifdef __cplusplus
}
#endif

#endif /* CORE_ERROR_MSG_H */

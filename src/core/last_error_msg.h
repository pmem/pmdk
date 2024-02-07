/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2014-2024, Intel Corporation */

/*
 * last_error_msg.h -- definitions for the "last_error_msg" module
 */

#ifndef CORE_LAST_ERROR_MSG_H
#define CORE_LAST_ERROR_MSG_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef NO_LIBPTHREAD
#define CORE_LAST_ERROR_MSG_MAXPRINT 8192 /* maximum expected log line */
#else
#define CORE_LAST_ERROR_MSG_MAXPRINT 256 /* ... for libpmem */
#endif

void last_error_msg_init(void);
void last_error_msg_fini(void);

const char *last_error_msg_get(void);

#ifdef __cplusplus
}
#endif

#endif /* CORE_LAST_ERROR_MSG_H */

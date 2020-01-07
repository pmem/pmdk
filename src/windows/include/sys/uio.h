// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2018, Intel Corporation */

/*
 * sys/uio.h -- definition of iovec structure
 */

#ifndef SYS_UIO_H
#define SYS_UIO_H 1

#include <pmemcompat.h>

#ifdef __cplusplus
extern "C" {
#endif

ssize_t writev(int fd, const struct iovec *iov, int iovcnt);

#ifdef __cplusplus
}
#endif

#endif /* SYS_UIO_H */

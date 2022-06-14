/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2017-2021, Intel Corporation */

/*
 * errno_freebsd.h -- map Linux errno's to something close on FreeBSD
 */

#ifndef MINIASYNC_ERRNO_FREEBSD_H
#define MINIASYNC_ERRNO_FREEBSD_H 1

#ifdef __FreeBSD__
#define EBADFD EBADF
#define ELIBACC EINVAL
#define EMEDIUMTYPE EOPNOTSUPP
#define ENOMEDIUM ENODEV
#define EREMOTEIO EIO
#endif

#endif /* MINIASYNC_ERRNO_FREEBSD_H */

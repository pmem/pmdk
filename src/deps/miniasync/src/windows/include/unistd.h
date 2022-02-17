/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2015-2021, Intel Corporation */

/*
 * unistd.h -- compatibility layer for POSIX operating system API
 */

#ifndef UNISTD_H
#define UNISTD_H 1

#include <stdio.h>

#define _SC_PAGESIZE 0
#define _SC_NPROCESSORS_ONLN 1

#define R_OK 04
#define W_OK 02
#define X_OK 00 /* execute permission doesn't exist on Windows */
#define F_OK 00

#endif /* UNISTD_H */

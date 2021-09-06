/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2014-2020, Intel Corporation */

/*
 * convert.h -- pmempool convert command header file
 */

#include <sys/types.h>

int pmempool_convert_func(const char *appname, int argc, char *argv[]);
void pmempool_convert_help(const char *appname);

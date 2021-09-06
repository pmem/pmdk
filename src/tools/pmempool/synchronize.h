/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2016-2020, Intel Corporation */

/*
 * synchronize.h -- pmempool sync command header file
 */

int pmempool_sync_func(const char *appname, int argc, char *argv[]);
void pmempool_sync_help(const char *appname);

/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2014-2023, Intel Corporation */

/*
 * check.h -- pmempool check command header file
 */

#ifdef _WIN32
#define WIN_DEPR_STR "Windows support is deprecated."
#define WIN_DEPR_ATTR __declspec(deprecated(WIN_DEPR_STR))
#endif

#ifdef _WIN32
WIN_DEPR_ATTR
#endif
int pmempool_check_func(const char *appname, int argc, char *argv[]);
#ifdef _WIN32
WIN_DEPR_ATTR
#endif
void pmempool_check_help(const char *appname);

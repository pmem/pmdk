/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2016-2020, Intel Corporation */

/*
 * mocks_windows.h -- redefinitions of libc functions
 *
 * This file is Windows-specific.
 *
 * This file should be included (i.e. using Forced Include) by libpmem
 * files, when compiled for the purpose of pmem_map_file test.
 * It would replace default implementation with mocked functions defined
 * in pmem_map_file.c.
 *
 * These defines could be also passed as preprocessor definitions.
 */

#ifndef WRAP_REAL
#define os_posix_fallocate __wrap_os_posix_fallocate
#define os_ftruncate __wrap_os_ftruncate
#endif

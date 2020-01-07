// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016-2017, Intel Corporation */

/*
 * mocks_windows.h -- redefinitions of libc functions used in util_poolset
 *
 * This file is Windows-specific.
 *
 * This file should be included (i.e. using Forced Include) by libpmem
 * files, when compiled for the purpose of util_poolset test.
 * It would replace default implementation with mocked functions defined
 * in util_poolset.c.
 *
 * These defines could be also passed as preprocessor definitions.
 */

#ifndef WRAP_REAL_OPEN
#define os_open __wrap_os_open
#endif

#ifndef WRAP_REAL_FALLOCATE
#define os_posix_fallocate __wrap_os_posix_fallocate
#endif

#ifndef WRAP_REAL_PMEM
#define pmem_is_pmem __wrap_pmem_is_pmem
#endif

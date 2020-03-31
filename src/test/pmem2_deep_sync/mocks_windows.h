// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/*
 * mocks_dax_windows.h -- redefinitions of pmem2_flush_file_buffers_os
 * and pmem2_set_flush_fns
 *
 * This file is Windows-specific.
 *
 * This file should be included (i.e. using Forced Include) by libpmem2
 * files, when compiled for the purpose of pmem2_deep_sync test.
 * It would replace default implementation with mocked functions defined
 * in mocks_windows.c
 *
 * This WRAP_REAL define could also be passed as a preprocessor definition.
 */

#ifndef MOCKS_WINDOWS_H
#define MOCKS_WINDOWS_H 1

#include <windows.h>

#ifndef WRAP_REAL

#define pmem2_flush_file_buffers_os __wrap_pmem2_flush_file_buffers_os
#define pmem2_set_flush_fns __wrap_pmem2_set_flush_fns

#endif

#endif

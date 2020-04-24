// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2018-2020, Intel Corporation */

/*
 * mocks_windows.h -- redefinitions of dimm functions
 */

#ifndef WRAP_REAL
#define GetFinalPathNameByHandleW __wrap_GetFinalPathNameByHandleW
#define CreateFileW __wrap_CreateFileW
#define DeviceIoControl __wrap_DeviceIoControl
#endif

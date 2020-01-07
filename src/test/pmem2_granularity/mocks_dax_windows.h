// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019, Intel Corporation */

/*
 * mocks_dax_windows.h -- redefinitions of GetVolumeInformationByHandleW
 *
 * This file is Windows-specific.
 *
 * This file should be included (i.e. using Forced Include) by libpmem2
 * files, when compiled for the purpose of pmem2_granularity test.
 * It would replace default implementation with mocked functions defined
 * in mocks_windows.c
 *
 * This WRAP_REAL define could also be passed as a preprocessor definition.
 */

#ifndef MOCKS_WINDOWS_H
#define MOCKS_WINDOWS_H 1

#include <windows.h>

#ifndef WRAP_REAL
#define GetVolumeInformationByHandleW __wrap_GetVolumeInformationByHandleW
BOOL
__wrap_GetVolumeInformationByHandleW(HANDLE hFile, LPWSTR lpVolumeNameBuffer,
	DWORD nVolumeNameSize, LPDWORD lpVolumeSerialNumber,
	LPDWORD lpMaximumComponentLength, LPDWORD lpFileSystemFlags,
	LPWSTR lpFileSystemNameBuffer, DWORD nFileSystemNameSize);
#endif

#endif

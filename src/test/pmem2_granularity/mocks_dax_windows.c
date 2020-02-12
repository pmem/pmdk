// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019, Intel Corporation */

/*
 * mocks_dax_windows.c -- mocked function required to control
 * FILE_DAX_VOLUME value reported by the OS APIs
 */

#include "unittest.h"

FUNC_MOCK_DLLIMPORT(GetVolumeInformationByHandleW, BOOL,
	HANDLE hFile,
	LPWSTR lpVolumeNameBuffer,
	DWORD nVolumeNameSize,
	LPDWORD lpVolumeSerialNumber,
	LPDWORD lpMaximumComponentLength,
	LPDWORD lpFileSystemFlags,
	LPWSTR lpFileSystemNameBuffer,
	DWORD nFileSystemNameSize)
FUNC_MOCK_RUN_DEFAULT {
	size_t is_pmem = atoi(os_getenv("IS_PMEM"));
	if (is_pmem)
		*lpFileSystemFlags = FILE_DAX_VOLUME;
	else
		*lpFileSystemFlags = 0;
	return TRUE;
}
FUNC_MOCK_END

/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2018-2020, Intel Corporation */

/*
 * mocks_windows.h -- redefinitions of EnumSystemFirmwareTables and
 *						GetSystemFirmwareTable
 *
 * This file is Windows-specific.
 *
 * This file should be included (i.e. using Forced Include) by libpmem
 * files, when compiled for the purpose of pmem_has_auto_flush_win test.
 * It would replace default implementation with mocked functions defined
 * in mocks_windows.c
 *
 * This WRAP_REAL define could be also passed as preprocessor definition.
 */
#include <windows.h>

#ifndef WRAP_REAL
#define EnumSystemFirmwareTables __wrap_EnumSystemFirmwareTables
#define GetSystemFirmwareTable __wrap_GetSystemFirmwareTable
UINT
__wrap_EnumSystemFirmwareTables(DWORD FirmwareTableProviderSignature,
	PVOID pFirmwareTableEnumBuffer, DWORD BufferSize);
UINT
__wrap_GetSystemFirmwareTable(DWORD FirmwareTableProviderSignature,
	DWORD FirmwareTableID, PVOID pFirmwareTableBuffer, DWORD BufferSize);
#endif

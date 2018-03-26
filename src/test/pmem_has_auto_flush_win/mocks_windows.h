/*
 * Copyright 2018, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *
 *     * Neither the name of the copyright holder nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * mocks_windows.h -- redefinitions of EnumSystemFirmwareTables and
 *						GetSystemFirmwareTable
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
#include <windows.h>

#ifndef WRAP_REAL
#define EnumSystemFirmwareTables __wrap_EnumSystemFirmwareTables
#define GetSystemFirmwareTable __wrap_GetSystemFirmwareTable
UINT
__wrap_EnumSystemFirmwareTables(DWORD FirmwareTableProviderSignature,
	PVOID pFirmwareTableEnumBuffer,	DWORD BufferSize);
UINT
__wrap_GetSystemFirmwareTable(DWORD FirmwareTableProviderSignature,
	DWORD FirmwareTableID, PVOID pFirmwareTableBuffer, DWORD BufferSize);
#endif

/*
 * Copyright 2019, Intel Corporation
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

/*
 * Copyright 2018-2019, Intel Corporation
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
 * mocks_windows.c -- mocked functions required to control the eADR and
 * FILE_DAX_VOLUME values reported by the OS APIs
 */

#include "util.h"
#include "unittest.h"

#include "set.h"
#include "stdlib.h"
#include "pmemcommon.h"
#include "auto_flush_windows.h"

size_t Is_nfit = 1;
size_t Pc_type = 7;
size_t Pc_capabilities;

/*
 * is_eadr -- set variable required for mocked functions
 */
static void
is_eadr()
{
	size_t is_eadr = atoi(os_getenv("IS_EADR"));
	if (is_eadr)
		Pc_capabilities = 3;
	else
		Pc_capabilities = 2;
}

FUNC_MOCK_DLLIMPORT(EnumSystemFirmwareTables, UINT,
				DWORD FirmwareTableProviderSignature,
				PVOID pFirmwareTableBuffer,
				DWORD BufferSize)
FUNC_MOCK_RUN_DEFAULT {
	if (FirmwareTableProviderSignature != ACPI_SIGNATURE)
		return _FUNC_REAL(EnumSystemFirmwareTables)
			(FirmwareTableProviderSignature,
				pFirmwareTableBuffer, BufferSize);
	if (Is_nfit == 1 && pFirmwareTableBuffer != NULL &&
			BufferSize != 0) {
		UT_OUT("Mock NFIT available");
		strncpy(pFirmwareTableBuffer, NFIT_STR_SIGNATURE, BufferSize);
	}
	return NFIT_SIGNATURE_LEN + sizeof(struct nfit_header);
}
FUNC_MOCK_END

FUNC_MOCK_DLLIMPORT(GetSystemFirmwareTable, UINT,
	DWORD FirmwareTableProviderSignature,
	DWORD FirmwareTableID,
	PVOID pFirmwareTableBuffer,
	DWORD BufferSize)
FUNC_MOCK_RUN_DEFAULT {
	is_eadr();
	if (FirmwareTableProviderSignature != ACPI_SIGNATURE ||
		FirmwareTableID != NFIT_REV_SIGNATURE)
		return _FUNC_REAL(GetSystemFirmwareTable)
			(FirmwareTableProviderSignature, FirmwareTableID,
				pFirmwareTableBuffer, BufferSize);
	if (pFirmwareTableBuffer == NULL && BufferSize == 0) {
		UT_OUT("GetSystemFirmwareTable mock");
		return sizeof(struct platform_capabilities) +
			sizeof(struct nfit_header);
	}
	struct nfit_header nfit;
	struct platform_capabilities pc;

	/* fill nfit */
	char sig[NFIT_SIGNATURE_LEN] = NFIT_STR_SIGNATURE;
	strncpy(nfit.signature, sig, NFIT_SIGNATURE_LEN);
	nfit.length = sizeof(nfit);
	memcpy(pFirmwareTableBuffer, &nfit, nfit.length);

	/* fill platform_capabilities */
	pc.length = sizeof(pc);
	/* [...] 0000 0011 - proper capabilities bits combination */
	pc.capabilities = (uint32_t)Pc_capabilities;
	pc.type = (uint16_t)Pc_type;
	memcpy((char *)pFirmwareTableBuffer + nfit.length, &pc, pc.length);

	return (sizeof(struct platform_capabilities) +
			sizeof(struct nfit_header));
}
FUNC_MOCK_END

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

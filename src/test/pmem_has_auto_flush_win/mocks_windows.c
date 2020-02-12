// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2018-2019, Intel Corporation */

/*
 * mocks_windows.c -- mocked functions used in auto_flush_windows.c
 */

#include "util.h"
#include "unittest.h"

#include "set.h"
#include "pmemcommon.h"
#include "auto_flush_windows.h"
#include "pmem_has_auto_flush_win.h"
#include <errno.h>

extern size_t Is_nfit;
extern size_t Pc_type;
extern size_t Pc_capabilities;

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

	return BufferSize;
}
FUNC_MOCK_END

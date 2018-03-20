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
 * os_auto_flush_windows.c -- Windows abstraction layer for auto flush detection
 */

#include <windows.h>
#include "out.h"
#include "os.h"

#define ACPI_SIGNATURE 'ACPI'
#define NFIT_NAME "NFIT"
#define NFIT_SIGNATURE_LEN 4

 /*
 * os_auto_flush -- check if platform supports auto flush
 */
int
is_nfit_available()
{
	LOG(3, "is_nfit_available");

	DWORD signatures_size;
	char *signatures;
	int isNFIT = 0;
	DWORD offset = 0;

	signatures_size = EnumSystemFirmwareTables(ACPI_SIGNATURE, NULL, 0);
	if (signatures_size == 0) {
		ERR("!EnumSystemFirmwareTables");
		goto err;
	}
	signatures = (char *)malloc(signatures_size);
	if (signatures == NULL) {
		ERR("!malloc");
		goto err;
	}
	int ret = EnumSystemFirmwareTables(ACPI_SIGNATURE,
										signatures, signatures_size);
	if (ret != signatures_size || ret == 0) {
		ERR("!EnumSystemFirmwareTables");
		goto err;
	}

	while (offset <= signatures_size) {
		int cmp = strncmp(signatures + offset, NFIT_NAME, NFIT_SIGNATURE_LEN);
		if (cmp == 0) {
			isNFIT = 1;
			goto end;
		}
		offset += NFIT_SIGNATURE_LEN;
	}

end:
	free(signatures);
	return isNFIT;

err:
	if (signatures)
		free(signatures);
	return -1;
}

/*
 * os_auto_flush -- check if platform supports auto flush
 */
int
os_auto_flush(void)
{
	LOG(15, NULL);

	return 0;
}

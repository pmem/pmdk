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
 * auto_flush_windows.c -- Windows auto flush detection
 */

#include <windows.h>
#include <inttypes.h>

#include "alloc.h"
#include "out.h"
#include "os.h"
#include "endian.h"
#include "auto_flush_windows.h"

/*
 * is_nfit_available -- (internal) check if platform supports NFIT table.
 */
static int
is_nfit_available()
{
	LOG(3, "is_nfit_available()");

	DWORD signatures_size;
	char *signatures = NULL;
	int is_nfit = 0;
	DWORD offset = 0;

	signatures_size = EnumSystemFirmwareTables(ACPI_SIGNATURE, NULL, 0);
	if (signatures_size == 0) {
		ERR("!EnumSystemFirmwareTables");
		return -1;
	}
	signatures = (char *)Malloc(signatures_size + 1);
	if (signatures == NULL) {
		ERR("!malloc");
		return -1;
	}
	int ret = EnumSystemFirmwareTables(ACPI_SIGNATURE,
					signatures, signatures_size);
	signatures[signatures_size] = '\0';
	if (ret != signatures_size) {
		ERR("!EnumSystemFirmwareTables");
		goto err;
	}

	while (offset <= signatures_size) {
		int nfit_sig  = strncmp(signatures + offset,
				NFIT_STR_SIGNATURE, NFIT_SIGNATURE_LEN);
		if (nfit_sig == 0) {
			is_nfit = 1;
			break;
		}
		offset += NFIT_SIGNATURE_LEN;
	}

	Free(signatures);
	return is_nfit;

err:
	Free(signatures);
	return -1;
}

/*
 * is_auto_flush_cap_set -- (internal) check if specific
 *                           capabilities bits are set.
 *
 * ACPI 6.2A Specification:
 * Bit[0] - CPU Cache Flush to NVDIMM Durability on
 * Power Loss Capable. If set to 1, indicates that platform
 * ensures the entire CPU store data path is flushed to
 * persistent memory on system power loss.
 * Bit[1] - Memory Controller Flush to NVDIMM Durability on Power Loss Capable.
 * If set to 1, indicates that platform provides mechanisms to automatically
 * flush outstanding write data from the memory controller to persistent memory
 * in the event of platform power loss. Note: If bit 0 is set to 1 then this bit
 * shall be set to 1 as well.
 */
static int
is_auto_flush_cap_set(uint32_t capabilities)
{
	LOG(3, "is_auto_flush_cap_set capabilities 0x%" PRIx32, capabilities);

	int CPU_cache_flush = CHECK_BIT(capabilities, 0);
	int memory_controller_flush = CHECK_BIT(capabilities, 1);

	LOG(15, "CPU_cache_flush %d, memory_controller_flush %d",
			CPU_cache_flush, memory_controller_flush);
	if (memory_controller_flush == 1 && CPU_cache_flush == 1)
		return 1;

	return 0;
}

/*
 * parse_nfit_buffer -- (internal) parse nfit buffer
 * if platform_capabilities struct is available return pcs structure.
 */
static struct platform_capabilities
parse_nfit_buffer(const unsigned char *nfit_buffer, unsigned long buffer_size)
{
	LOG(3, "parse_nfit_buffer nfit_buffer %s, buffer_size %lu",
			nfit_buffer, buffer_size);

	uint16_t type;
	uint16_t length;
	size_t offset = sizeof(struct nfit_header);
	struct platform_capabilities pcs = {0};

	while (offset < buffer_size) {
		type = *(nfit_buffer + offset);
		length = *(nfit_buffer + offset + 2);
		if (type == PCS_TYPE_NUMBER) {
			if (length == sizeof(struct platform_capabilities)) {
				memmove(&pcs, nfit_buffer + offset, length);
				return pcs;
			}
		}
		offset += length;
	}
	return pcs;
}

/*
 * pmem2_auto_flush -- check if platform supports auto flush.
 */
int
pmem2_auto_flush(void)
{
	LOG(3, NULL);

	DWORD nfit_buffer_size = 0;
	DWORD nfit_written = 0;
	PVOID nfit_buffer = NULL;
	struct nfit_header *nfit_data;
	struct platform_capabilities *pc = NULL;

	int eADR = 0;
	int is_nfit = is_nfit_available();
	if (is_nfit == 0) {
		LOG(15, "ACPI NFIT table not available");
		return 0;
	}
	if (is_nfit < 0 || is_nfit != 1) {
		LOG(1, "!is_nfit_available");
		return -1;
	}

	/* get the entire nfit size */
	nfit_buffer_size = GetSystemFirmwareTable(
		(DWORD)ACPI_SIGNATURE, (DWORD)NFIT_REV_SIGNATURE, NULL, 0);
	if (nfit_buffer_size == 0) {
		ERR("!GetSystemFirmwareTable");
		return -1;
	}
	/* reserve buffer */
	nfit_buffer = (unsigned char *)Malloc(nfit_buffer_size);
	if (nfit_buffer == NULL) {
		ERR("!malloc");
		goto err;
	}
	/* write actual nfit to buffer */
	nfit_written = GetSystemFirmwareTable(
		(DWORD)ACPI_SIGNATURE, (DWORD)NFIT_REV_SIGNATURE,
		nfit_buffer, nfit_buffer_size);
	if (nfit_written == 0) {
		ERR("!GetSystemFirmwareTable");
		goto err;
	}

	if (nfit_buffer_size != nfit_written) {
		errno = ERROR_INVALID_DATA;
		ERR("!GetSystemFirmwareTable invalid data");
		goto err;
	}

	nfit_data = (struct nfit_header *)nfit_buffer;
	int nfit_sig = strncmp(nfit_data->signature,
			NFIT_STR_SIGNATURE, NFIT_SIGNATURE_LEN);
	if (nfit_sig != 0) {
		ERR("!NFIT buffer has invalid data");
		goto err;
	}

	struct platform_capabilities pcs = parse_nfit_buffer(
			nfit_buffer, nfit_buffer_size);
	eADR = is_auto_flush_cap_set(pcs.capabilities);

	Free(nfit_buffer);
	return eADR;

err:
	Free(nfit_buffer);
	return -1;
}

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
#include <inttypes.h>
#include "out.h"
#include "os.h"
#include "endian.h"

#define ACPI_SIGNATURE  "ACPI"
#define NFIT_SIGNATURE "NFIT"
#define NFIT_SIGNATURE_LEN 4

#define NFIT_SIGNATURE_LEN 4
#define NFIT_OEM_ID_LEN 6
#define NFIT_OEM_TABLE_ID_LEN 8
#define NFIT_MAX_STRUCTURES 8

#define PCS_RESERVED 3
#define PCS_RESERVED_2 4
#define PCS_TYPE_NUMBER 7

#define CHECK_BIT(num, bit) (((num) >> (bit)) & 1)
#define PACK_STRUCT(_structure_) \
			__pragma(pack(push, 1)) _structure_; __pragma(pack(pop))

PACK_STRUCT(
struct platform_capabilities
{
	uint16_t type;
	uint16_t length;
	uint8_t highest_valid;
	uint8_t reserved[PCS_RESERVED];
	uint32_t capabilities;
	uint8_t reserved2[PCS_RESERVED_2];
})

PACK_STRUCT(
struct nfit_header
{
	char signature[NFIT_SIGNATURE_LEN];
	unsigned length;
	unsigned char revision;
	unsigned char checksum;
	unsigned char oem_id[NFIT_OEM_ID_LEN];
	unsigned char oem_table_id[NFIT_OEM_TABLE_ID_LEN];
	unsigned oem_revision;
	unsigned char creator_id[4];
	unsigned creator_revision;
	unsigned reserved;
})

/*
 * check_nfit_signature -- (internal) check if string is NFIT signature.
 */
static int
check_nfit_signature(const char *str)
{
	LOG(3, "check_nfit_signature str %s", str);

	int cmp = strncmp(str, NFIT_SIGNATURE, NFIT_SIGNATURE_LEN);
	if (cmp == 0)
		return 1;

	return 0;
}

/*
 * is_nfit_available -- (internal) check if platform supports NFIT table.
 */
static int
is_nfit_available()
{
	LOG(3, "is_nfit_available()");

	DWORD signatures_size;
	char *signatures;
	int is_nfit = 0;
	DWORD offset = 0;

	DWORD acpi_dword_be = htobe32(util_string_to_dword(ACPI_SIGNATURE));
	signatures_size = EnumSystemFirmwareTables(acpi_dword_be, NULL, 0);
	if (signatures_size == 0) {
		ERR("!EnumSystemFirmwareTables");
		goto err;
	}
	signatures = (char *)malloc(signatures_size);
	if (signatures == NULL) {
		ERR("!malloc");
		goto err;
	}
	int ret = EnumSystemFirmwareTables(acpi_dword_be,
					signatures, signatures_size);
	if (ret != signatures_size || ret == 0) {
		ERR("!EnumSystemFirmwareTables");
		goto err;
	}

	while (offset <= signatures_size) {
		int nfit_sig = check_nfit_signature(signatures + offset);
		if (nfit_sig) {
			is_nfit = 1;
			goto end;
		}
		offset += NFIT_SIGNATURE_LEN;
	}

end:
	free(signatures);
	return is_nfit;

err:
	if (signatures)
		free(signatures);
	return -1;
}

/*
 * check_capabilities -- (internal) check if specific capabilities bits are set.
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
check_capabilities(uint32_t capabilities)
{
	LOG(3, "check_capabilities capabilities %" PRIu32, capabilities);

	int CPU_cache_flush = CHECK_BIT(capabilities, 0);
	int memory_controller_flush = CHECK_BIT(capabilities, 1);
	int supported = 0;

	LOG(15, "CPU_cache_flush %d, memory_controller_flush %d",
			CPU_cache_flush, memory_controller_flush);
	if (memory_controller_flush == 1 && CPU_cache_flush == 1)
		supported = 1;

	return supported;
}

/*
 * parse_nfit_buffer -- (internal) parse nfit buffer
 * if platform_capabilities struct is available fill up pc structure.
 */
static void
parse_nfit_buffer(unsigned char *nfit_buffer, unsigned long buffer_size,
					struct platform_capabilities *pc)
{
	LOG(3, "parse_nfit_buffer nfit_buffer %s, buffer_size %lu, pc %p",
			nfit_buffer, buffer_size, pc);

	uint16_t type;
	uint16_t length;
	size_t offset = sizeof(struct nfit_header);

	while (offset < buffer_size) {
		type = *(nfit_buffer + offset);
		length = *(nfit_buffer + offset + 2);
		if (type == PCS_TYPE_NUMBER) {

			if (length == sizeof(struct platform_capabilities)) {
				memmove(pc, nfit_buffer + offset, length);
				return;
			}
		}
		offset += length;
	}
	pc = NULL;
}

/*
 * os_auto_flush -- check if platform supports auto flush.
 */
int
os_auto_flush(void)
{
	LOG(3, NULL);

	int eADR = 0;
	int is_nfit = is_nfit_available();
	if (is_nfit == 0) {
		LOG(15, "ACPI NFIT table not available");
		goto not_supported;
	} else if (is_nfit < 0 || is_nfit != 1) {
		goto err;
	}

	DWORD acpi_dword = util_string_to_dword(ACPI_SIGNATURE);
	DWORD nfit_dword = util_string_to_dword(NFIT_SIGNATURE);
	DWORD acpi_dword_be = htobe32(acpi_dword);
	DWORD nfit_buffer_size = 0;
	DWORD nfit_written = 0;
	unsigned char *nfit_buffer;
	struct nfit_header nfit_data;
	struct platform_capabilities *pc = NULL;

	/* get the entire nfit size */
	nfit_buffer_size = GetSystemFirmwareTable(acpi_dword_be, nfit_dword,
							NULL, 0);
	if (nfit_buffer_size == 0) {
		ERR("!GetSystemFirmwareTable");
		goto err;
	}
	/* reserve buffer */
	nfit_buffer = (unsigned char *)malloc(nfit_buffer_size);
	if (nfit_buffer == NULL) {
		ERR("!malloc");
		goto err;
	}
	/* write actual nfit to buffer */
	nfit_written = GetSystemFirmwareTable(acpi_dword_be, nfit_dword,
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

	memcpy(&nfit_data, nfit_buffer, sizeof(struct nfit_header));
	int nfit_sig = check_nfit_signature(nfit_data.signature);
	if (nfit_sig == 0) {
		ERR("!NFIT buffer has invalid data");
		goto err;
	}

	/* read  platform capabilities structure */
	pc = malloc(sizeof(struct platform_capabilities));
	parse_nfit_buffer(nfit_buffer, nfit_buffer_size, pc);

	if (pc)
		eADR = check_capabilities(pc->capabilities);

	free(pc);
	free(nfit_buffer);

not_supported:
	return eADR;

err:
	if (nfit_buffer)
		free(nfit_buffer);
	return -1;
}

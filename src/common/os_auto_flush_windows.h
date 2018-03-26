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

#ifndef PMDK_OS_AUTO_FLUSH_WINDOWS_H
#define PMDK_OS_AUTO_FLUSH_WINDOWS_H 1

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

/* check if bit on 'bit' position in number 'num' is set */
#define CHECK_BIT(num, bit) (((num) >> (bit)) & 1)
/*
 * sets alignment of members of structure,
 * pushes and pop alignment setting on an internal stack
 */
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
#endif

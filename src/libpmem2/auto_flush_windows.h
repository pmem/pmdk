/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2018-2020, Intel Corporation */

#ifndef PMEM2_AUTO_FLUSH_WINDOWS_H
#define PMEM2_AUTO_FLUSH_WINDOWS_H 1

#define ACPI_SIGNATURE 0x41435049 /* hex value of ACPI signature */
#define NFIT_REV_SIGNATURE 0x5449464e /* hex value of htonl(NFIT) signature */
#define NFIT_STR_SIGNATURE "NFIT"

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
 * sets alignment of members of structure
 */
#pragma pack(1)
struct platform_capabilities
{
	uint16_t type;
	uint16_t length;
	uint8_t highest_valid;
	uint8_t reserved[PCS_RESERVED];
	uint32_t capabilities;
	uint8_t reserved2[PCS_RESERVED_2];
};

struct nfit_header
{
	uint8_t signature[NFIT_SIGNATURE_LEN];
	uint32_t length;
	uint8_t revision;
	uint8_t checksum;
	uint8_t oem_id[NFIT_OEM_ID_LEN];
	uint8_t oem_table_id[NFIT_OEM_TABLE_ID_LEN];
	uint32_t oem_revision;
	uint8_t creator_id[4];
	uint32_t creator_revision;
	uint32_t reserved;
};
#pragma pack()
#endif

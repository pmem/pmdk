// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2014-2018, Intel Corporation */

/*
 * uuid.h -- internal definitions for uuid module
 */

#ifndef PMDK_UUID_H
#define PMDK_UUID_H 1

#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Structure for binary version of uuid. From RFC4122,
 * https://tools.ietf.org/html/rfc4122
 */
struct uuid {
	uint32_t time_low;
	uint16_t time_mid;
	uint16_t time_hi_and_ver;
	uint8_t clock_seq_hi;
	uint8_t	clock_seq_low;
	uint8_t node[6];
};

#define POOL_HDR_UUID_LEN	16 /* uuid byte length */
#define POOL_HDR_UUID_STR_LEN	37 /* uuid string length */
#define POOL_HDR_UUID_GEN_FILE	"/proc/sys/kernel/random/uuid"

typedef unsigned char uuid_t[POOL_HDR_UUID_LEN]; /* 16 byte binary uuid value */

int util_uuid_generate(uuid_t uuid);
int util_uuid_to_string(const uuid_t u, char *buf);
int util_uuid_from_string(const char uuid[POOL_HDR_UUID_STR_LEN],
	struct uuid *ud);

/*
 * uuidcmp -- compare two uuids
 */
static inline int
uuidcmp(const uuid_t uuid1, const uuid_t uuid2)
{
	return memcmp(uuid1, uuid2, POOL_HDR_UUID_LEN);
}

#ifdef __cplusplus
}
#endif

#endif

/*
 * Copyright 2014-2019, Intel Corporation
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

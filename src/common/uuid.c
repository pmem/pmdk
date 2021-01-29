/*
 * Copyright 2014-2018, Intel Corporation
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
 * uuid.c -- uuid utilities
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "uuid.h"
#include "out.h"

/*
 * util_uuid_to_string -- generate a string form of the uuid
 */
int
util_uuid_to_string(const uuid_t u, char *buf)
{
	int len; /* size that is returned from sprintf call */

	if (buf == NULL) {
		LOG(2, "invalid buffer for uuid string");
		return -1;
	}

	if (u == NULL) {
		LOG(2, "invalid uuid structure");
		return -1;
	}

	struct uuid *uuid = (struct uuid *)u;
	len = snprintf(buf, POOL_HDR_UUID_STR_LEN,
		"%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
		uuid->time_low, uuid->time_mid, uuid->time_hi_and_ver,
		uuid->clock_seq_hi, uuid->clock_seq_low, uuid->node[0],
		uuid->node[1], uuid->node[2], uuid->node[3], uuid->node[4],
		uuid->node[5]);

	if (len != POOL_HDR_UUID_STR_LEN - 1) {
		LOG(2, "snprintf(uuid): %d", len);
		return -1;
	}

	return 0;
}

/*
 * util_uuid_from_string -- generate a binary form of the uuid
 *
 * uuid string read from /proc/sys/kernel/random/uuid. UUID string
 * format example:
 * f81d4fae-7dec-11d0-a765-00a0c91e6bf6
 */
int
util_uuid_from_string(const char uuid[POOL_HDR_UUID_STR_LEN], struct uuid *ud)
{
	if (strlen(uuid) != 36) {
		LOG(2, "invalid uuid string");
		return -1;
	}

	if (uuid[8] != '-' || uuid[13] != '-' || uuid[18] != '-' ||
			uuid[23] != '-') {
		LOG(2, "invalid uuid string");
		return -1;
	}

	int n = sscanf(uuid,
		"%08x-%04hx-%04hx-%02hhx%02hhx-"
		"%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx",
		&ud->time_low, &ud->time_mid, &ud->time_hi_and_ver,
		&ud->clock_seq_hi, &ud->clock_seq_low, &ud->node[0],
		&ud->node[1], &ud->node[2], &ud->node[3], &ud->node[4],
		&ud->node[5]);

	if (n != 11) {
		LOG(2, "sscanf(uuid)");
		return -1;
	}

	return 0;
}

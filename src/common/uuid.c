// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2014-2018, Intel Corporation */

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
util_uuid_from_string(const char *uuid, struct uuid *ud)
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

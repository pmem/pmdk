// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2024, Intel Corporation */

/*
 * uuid_linux.c -- pool set utilities with OS-specific implementation
 */

#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>

#include "uuid.h"
#include "os.h"
#include "log_internal.h"

/*
 * util_uuid_generate -- generate a uuid
 *
 * This function reads the uuid string from  /proc/sys/kernel/random/uuid
 * It converts this string into the binary uuid format as specified in
 * https://www.ietf.org/rfc/rfc4122.txt
 */
int
util_uuid_generate(uuid_t uuid)
{
	char uu[POOL_HDR_UUID_STR_LEN];

	int fd = os_open(POOL_HDR_UUID_GEN_FILE, O_RDONLY);
	if (fd < 0) {
		/* Fatal error */
		CORE_LOG_ERROR_W_ERRNO("open(uuid)");
		return -1;
	}
	ssize_t num = read(fd, uu, POOL_HDR_UUID_STR_LEN);
	if (num < POOL_HDR_UUID_STR_LEN) {
		/* Fatal error */
		CORE_LOG_ERROR_W_ERRNO("read(uuid)");
		os_close(fd);
		return -1;
	}
	os_close(fd);

	uu[POOL_HDR_UUID_STR_LEN - 1] = '\0';
	int ret = util_uuid_from_string(uu, (struct uuid *)uuid);
	if (ret < 0)
		return ret;

	return 0;
}

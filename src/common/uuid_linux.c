/*
 * Copyright 2015-2017, Intel Corporation
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
 * uuid_linux.c -- pool set utilities with OS-specific implementation
 */

#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>

#include "uuid.h"
#include "os.h"
#include "out.h"

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
		LOG(2, "!open(uuid)");
		return -1;
	}
	ssize_t num = read(fd, uu, POOL_HDR_UUID_STR_LEN);
	if (num < POOL_HDR_UUID_STR_LEN) {
		/* Fatal error */
		LOG(2, "!read(uuid)");
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

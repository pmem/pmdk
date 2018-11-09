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

#include <cassert>
#include <fcntl.h>
#include <file.h>

#include "os.h"
#include "poolset_util.hpp"
#include "set.h"

#define PART_TEMPLATE "part."
#define POOL_PART_SIZE (1UL << 30)

/*
 * dynamic_poolset_clear -- clears header in first part if it exists
 */
static int
dynamic_poolset_clear(const char *dir)
{
	char path[PATH_MAX];
	int count = snprintf(path, sizeof(path),
			     "%s" OS_DIR_SEP_STR PART_TEMPLATE "0", dir);
	assert(count > 0);
	if ((size_t)count >= sizeof(path)) {
		fprintf(stderr, "path to a poolset part too long\n");
		return -1;
	}

	int exists = util_file_exists(path);
	if (exists < 0)
		return -1;

	if (!exists)
		return 0;

	return util_file_zero(path, 0, POOL_HDR_SIZE);
}

/*
 * dynamic_poolset_create -- clear pool's header and create new poolset
 */
int
dynamic_poolset_create(const char *path, size_t size)
{
	/* buffer for part's path and size */
	char buff[PATH_MAX + 20];

	int ret;
	int fd;
	int count;
	int curr_part = 0;

	ret = dynamic_poolset_clear(path);
	if (ret == -1)
		return -1;

	fd = os_open(POOLSET_PATH, O_RDWR | O_CREAT, 0644);
	if (fd == -1) {
		perror("open");
		return -1;
	}

	char header[] = "PMEMPOOLSET\nOPTION SINGLEHDR\n";

	ret = util_write_all(fd, header, sizeof(header) - 1);
	if (ret == -1)
		goto err;

	while (curr_part * POOL_PART_SIZE < size + POOL_HDR_SIZE) {
		count = snprintf(buff, sizeof(buff),
				 "%lu %s" OS_DIR_SEP_STR PART_TEMPLATE "%d\n",
				 POOL_PART_SIZE, path, curr_part);
		assert(count > 0);
		if ((size_t)count >= sizeof(buff)) {
			fprintf(stderr, "path to a poolset part too long\n");
			goto err;
		}

		ret = util_write_all(fd, buff, count);
		if (ret == -1)
			goto err;

		curr_part++;
	}

	close(fd);
	return 0;

err:
	close(fd);
	return -1;
}

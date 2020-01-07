// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2018, Intel Corporation */

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

// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021, Intel Corporation */

/*
 * file_posix.c -- implementation of file API (posix)
 */

#include <fcntl.h>
#include <libpmem2.h>
#include <stdbool.h>

#include "file.h"
#include "libpmemset.h"
#include "pmemset_utils.h"
#include "config.h"

/*
 * pmemset_file_create_pmem2_src -- create pmem2_source structure based on the
 *                                  provided path to the file
 */
int
pmemset_file_create_pmem2_src(struct pmem2_source **pmem2_src, char *path,
		struct pmemset_config *cfg)
{
	/* Init open parameters */
	int access = O_RDWR;
	mode_t mode = (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

	/* Get file create disposition */
	enum pmemset_config_file_create_disposition file_disposition =
			pmemset_config_get_file_create_disposition(cfg);

	switch (file_disposition) {
		case PMEMSET_CONFIG_FILE_CREATE_ALWAYS:
			access |= O_TRUNC;
		case PMEMSET_CONFIG_FILE_CREATE_IF_NEEDED:
			access |= O_CREAT;
			break;
		case PMEMSET_CONFIG_FILE_OPEN:
			break;
		default:
			ERR("unknown file create disposition value %d",
				file_disposition);
			return PMEMSET_E_INVALID_CFG_FILE_CREATE_DISP;
	}

	int fd = os_open(path, access, mode);
	if (fd < 0) {
		ERR("!open %s", path);
		return PMEMSET_E_ERRNO;
	}

	int ret = pmem2_source_from_fd(pmem2_src, fd);
	if (ret)
		goto err_close_file;

	return 0;

err_close_file:
	os_close(fd);
	return ret;
}

/*
 * pmemset_file_close -- closes the file described by the file descriptor
 */
int
pmemset_file_close(struct pmem2_source *pmem2_src)
{
	int fd;
	int ret = pmem2_source_get_fd(pmem2_src, &fd);
	if (ret)
		return ret;

	ret = os_close(fd);
	if (ret) {
		ERR("!close");
		return PMEMSET_E_ERRNO;
	}

	return 0;
}

/*
 * pmemset_file_dispose_pmem2_src -- disposes of the pmem2_source structure
 */
int
pmemset_file_dispose_pmem2_src(struct pmem2_source **pmem2_src)
{
	int ret = pmemset_file_close(*pmem2_src);
	if (ret)
		return ret;

	return pmem2_source_delete(pmem2_src);
}

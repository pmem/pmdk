// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020-2021, Intel Corporation */

/*
 * file_posix.c -- implementation of file API (posix)
 */

#include <fcntl.h>
#include <libpmem2.h>
#include <stdbool.h>

#include "file.h"
#include "libpmemset.h"
#include "pmemset_utils.h"

/*
 * pmemset_file_create_pmem2_src -- create pmem2_source structure based on the
 *                                  provided path to the file
 */
int
pmemset_file_create_pmem2_src(struct pmem2_source **pmem2_src, char *path,
		uint64_t flags)
{
	/* Init open arguments */
	int access = O_RDWR;
	/* default mode */
	mode_t mode = (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	/* user requested mode */
	mode_t flag_mode = FILE_CREATE_MODE_FROM_FLAG(flags);

	/* Check create disposition flags */
	if (flags & PMEMSET_SOURCE_FILE_CREATE_ALWAYS)
		access |= (O_CREAT | O_TRUNC);
	else if (flags & PMEMSET_SOURCE_FILE_CREATE_IF_NEEDED)
		access |= (O_CREAT);

	if (flag_mode)
		mode = flag_mode;
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
 * pmemset_file_create_pmem2_src_from_temp -- create pmem2_source structure
 * based on the provided dir to temp file
 */
int
pmemset_file_create_pmem2_src_from_temp(struct pmem2_source **pmem2_src,
		char *dir)
{
	int fd = util_tmpfile(dir, OS_DIR_SEP_STR"pmemset.XXXXXX",
					O_CREAT & O_EXCL);
	if (fd < 0) {
		ERR("failed to create temporary file at \"%s\"", dir);
		return PMEMSET_E_CANNOT_CREATE_TEMP_FILE;
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

/*
 * pmemset_file_grow -- grow file from pmemset_file to a specified len
 */
int
pmemset_file_grow(struct pmemset_file *file, size_t len)
{
	int fd = pmemset_file_get_fd(file);
	int ret = os_ftruncate(fd, (os_off_t)len);
	if (ret < 0) {
		ERR("!ftruncate");
		return PMEMSET_E_ERRNO;
	}

	return ret;
}

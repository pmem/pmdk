// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019-2020, Intel Corporation */

#include <errno.h>
#include <fcntl.h>
#include "os.h"
#include "source.h"
#include "alloc.h"
#include "libpmem2.h"
#include "out.h"
#include "pmem2.h"
#include "pmem2_utils.h"

/*
 * pmem2_source_from_fd -- create a new data source instance
 */
int
pmem2_source_from_fd(struct pmem2_source **src, int fd)
{
	*src = NULL;

	if (fd < 0)
		return PMEM2_E_INVALID_FILE_HANDLE;

	int flags = fcntl(fd, F_GETFL);

	if (flags == -1) {
		ERR("!fcntl");
		if (errno == EBADF)
			return PMEM2_E_INVALID_FILE_HANDLE;
		return PMEM2_E_ERRNO;
	}

	if ((flags & O_ACCMODE) == O_WRONLY) {
		ERR("fd must be open with O_RDONLY or O_RDWR");
		return PMEM2_E_INVALID_FILE_HANDLE;
	}

	/*
	 * XXX Files with FS_APPEND_FL attribute should also generate an error.
	 * If it is possible to filter them out pmem2_map would not generate
	 * -EACCESS trying to map them. Please update pmem2_map.3 when it will
	 * be fixed. For details please see the ioctl_iflags(2) manual page.
	 */

	os_stat_t st;

	if (os_fstat(fd, &st) < 0) {
		ERR("!fstat");
		if (errno == EBADF)
			return PMEM2_E_INVALID_FILE_HANDLE;
		return PMEM2_E_ERRNO;
	}

	enum pmem2_file_type type;
	int ret = pmem2_get_type_from_stat(&st, &type);
	if (ret != 0)
		return ret;

	if (type == PMEM2_FTYPE_DIR) {
		ERR("cannot set fd to directory in pmem2_source_from_fd");
		return PMEM2_E_INVALID_FILE_TYPE;
	}

	struct pmem2_source *srcp = pmem2_malloc(sizeof(**src), &ret);
	if (ret)
		return ret;

	ASSERTne(srcp, NULL);

	srcp->fd = fd;
	*src = srcp;

	return 0;
}

/*
 * pmem2_source_size -- get a size of the file descriptor stored in the provided
 * source
 */
int
pmem2_source_size(const struct pmem2_source *src, size_t *size)
{
	LOG(3, "fd %d", src->fd);

	os_stat_t st;

	if (os_fstat(src->fd, &st) < 0) {
		ERR("!fstat");
		if (errno == EBADF)
			return PMEM2_E_INVALID_FILE_HANDLE;
		return PMEM2_E_ERRNO;
	}

	enum pmem2_file_type type;
	int ret = pmem2_get_type_from_stat(&st, &type);
	if (ret)
		return ret;

	switch (type) {
	case PMEM2_FTYPE_DEVDAX: {
		int ret = pmem2_device_dax_size_from_stat(&st, size);
		if (ret)
			return ret;
		break;
	}
	case PMEM2_FTYPE_REG:
		if (st.st_size < 0) {
			ERR(
				"kernel says size of regular file is negative (%ld)",
				st.st_size);
			return PMEM2_E_INVALID_FILE_HANDLE;
		}
		*size = (size_t)st.st_size;
		break;
	default:
		FATAL(
			"BUG: unhandled file type in pmem2_source_size");
	}

	LOG(4, "file length %zu", *size);
	return 0;
}

/*
 * pmem2_source_alignment -- get alignment from the file descriptor stored in
 * the provided source
 */
int
pmem2_source_alignment(const struct pmem2_source *src, size_t *alignment)
{
	LOG(3, "fd %d", src->fd);

	os_stat_t st;

	if (os_fstat(src->fd, &st) < 0) {
		ERR("!fstat");
		if (errno == EBADF)
			return PMEM2_E_INVALID_FILE_HANDLE;
		return PMEM2_E_ERRNO;
	}

	enum pmem2_file_type type;
	int ret = pmem2_get_type_from_stat(&st, &type);
	if (ret)
		return ret;

	switch (type) {
	case PMEM2_FTYPE_DEVDAX: {
		int ret = pmem2_device_dax_alignment_from_stat(&st, alignment);
		if (ret)
			return ret;
		break;
	}
	case PMEM2_FTYPE_REG:
		*alignment = Pagesize;
		break;
	default:
		FATAL(
			"BUG: unhandled file type in pmem2_source_alignment");
	}

	if (!util_is_pow2(*alignment)) {
		ERR("alignment (%zu) has to be a power of two", *alignment);
		return PMEM2_E_INVALID_ALIGNMENT_VALUE;
	}

	LOG(4, "alignment %zu", *alignment);

	return 0;
}

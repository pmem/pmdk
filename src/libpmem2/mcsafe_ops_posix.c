// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021, Intel Corporation */

#include <errno.h>

#include "libpmem2.h"
#include "out.h"
#include "pmem2_utils.h"
#include "source.h"

enum mcsafe_op {
	MCSAFE_OP_READ,
	MCSAFE_OP_WRITE,

	MAX_MCSAFE_OP,
};

/*
 * pmem2_source_mcsafe_operation -- execute operation in a safe manner
 *                                  (detect badblocks)
 */
static int
pmem2_source_mcsafe_operation(struct pmem2_source *src, void *buf, size_t size,
		size_t offset, enum mcsafe_op op_type)
{
	ASSERT(op_type >= 0 && op_type < MAX_MCSAFE_OP);

	if (src->type != PMEM2_SOURCE_FD) {
		ERR("operation doesn't support provided source type, only "\
			"source created from file descriptor is supported");
		return PMEM2_E_SOURCE_TYPE_NOT_SUPPORTED;
	}

	int fd;
	int ret = pmem2_source_get_fd(src, &fd);
	ASSERT(fd >= 0);

	size_t file_size;
	ret = pmem2_source_size(src, &file_size);
	if (ret)
		return ret;

	size_t max_size = (size_t)(file_size - offset);
	if (size > max_size) {
		if (op_type == MCSAFE_OP_READ)
			ERR("size of read %zu from offset %zu goes beyond " \
				"the file length %zu",
				size, offset, max_size);
		else if (op_type == MCSAFE_OP_WRITE)
			ERR("size of write %zu from offset %zu goes beyond " \
				"the file length %zu",
				size, offset, max_size);

		return PMEM2_E_LENGTH_OUT_OF_RANGE;
	}

	/* fsdax read/write relies on pread/pwrite */
	enum pmem2_file_type ftype = src->value.ftype;
	if (ftype == PMEM2_FTYPE_DIR || ftype == PMEM2_FTYPE_REG) {
		if (op_type == MCSAFE_OP_READ) {
			ssize_t retsize = pread(fd, buf, size, (off_t)offset);
			if (retsize == -1) {
				if (errno == EIO) {
					ERR("physical I/O error occured, " \
						"possible bad block encountered");
					return PMEM2_E_IO_FAIL;
				}

				ERR("!pread");
				return PMEM2_E_ERRNO;
			}
		} else if (op_type == MCSAFE_OP_WRITE) {
			ssize_t retsize = pwrite(fd, buf, size, (off_t)offset);
			if (retsize == -1) {
				if (errno == EIO) {
					ERR("physical I/O error occured, "
						"possible bad block encountered");
					return PMEM2_E_IO_FAIL;
				}

				ERR("!pwrite");
				return PMEM2_E_ERRNO;
			}
		}

		return 0;
	}

	/* TODO: devdax implementation of this function */
	if (ftype == PMEM2_FTYPE_DEVDAX) {
		ERR("operation doesn't supported devdax file type yet");
		return PMEM2_E_NOSUPP;
	}

	return ret;
}

/*
 * pmem2_source_pread_mcsafe -- read from the source in a safe manner
 *                              (detect badblocks)
 */
int
pmem2_source_pread_mcsafe(struct pmem2_source *src, void *buf, size_t size,
		size_t offset)
{
	LOG(3, "source %p buf %p size %zu offset %zu", src, buf, size, offset);
	PMEM2_ERR_CLR();

	return pmem2_source_mcsafe_operation(src, buf, size, offset,
			MCSAFE_OP_READ);
}

/*
 * pmem2_source_pwrite_mcsafe -- write from the source in a safe manner
 *                               (detect badblocks)
 */
int
pmem2_source_pwrite_mcsafe(struct pmem2_source *src, void *buf, size_t size,
		size_t offset)
{
	LOG(3, "source %p buf %p size %zu offset %zu", src, buf, size, offset);
	PMEM2_ERR_CLR();

	return pmem2_source_mcsafe_operation(src, buf, size, offset,
			MCSAFE_OP_WRITE);
}

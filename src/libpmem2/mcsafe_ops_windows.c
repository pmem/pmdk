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

	if (src->type != PMEM2_SOURCE_FD && src->type != PMEM2_SOURCE_HANDLE) {
		ERR("operation doesn't support provided source type, only "\
			"sources created from file descriptor or file handle "\
			"are supported");
		return PMEM2_E_SOURCE_TYPE_NOT_SUPPORTED;
	}

	HANDLE fh;
	int ret = pmem2_source_get_handle(src, &fh);
	ASSERTeq(ret, 0);

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

	enum pmem2_file_type ftype = src->value.ftype;
	ASSERT(ftype > 0 && ftype < MAX_PMEM2_FILE_TYPE);

	/* devdax on windows is not possible */
	ASSERTne(ftype, PMEM2_FTYPE_DEVDAX);

	/* source from directory file can't be created in pmem2 */
	ASSERTne(ftype, PMEM2_FTYPE_DIR);

	/* fsdax read/write relies on pread/pwrite */
	if (ftype == PMEM2_FTYPE_REG) {
		if (op_type == MCSAFE_OP_READ)
			ret = ReadFile(fh, buf, (DWORD)size, NULL, NULL);
		else if (op_type == MCSAFE_OP_WRITE)
			ret = WriteFile(fh, buf, (DWORD)size, NULL, NULL);

		if (ret == 0) {
			if (op_type == MCSAFE_OP_READ)
				ERR("!!ReadFile");
			else if (op_type == MCSAFE_OP_WRITE)
				ERR("!!WriteFile");

			return pmem2_lasterror_to_err();
		}

		return 0;
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

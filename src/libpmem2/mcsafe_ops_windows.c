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
 * mcsafe_op_none -- invalid safe operation definition
 */
static int
mcsafe_op_none(struct pmem2_source *src, void *buf, size_t size,
		size_t offset)
{
	/* suppres unused parameters */
	SUPPRESS_UNUSED(src, buf, size, offset);

	ASSERT(0);

	return PMEM2_E_NOSUPP;
}

/*
 * mcsafe_op_reg_read -- safe regular read operation
 */
static int
mcsafe_op_reg_read(struct pmem2_source *src, void *buf, size_t size,
		size_t offset)
{
	HANDLE fh;
	int ret = pmem2_source_get_handle(src, &fh);
	ASSERTeq(ret, 0);

	ret = ReadFile(fh, buf, (DWORD)size, NULL, NULL);
	if (ret == 0) {
		ERR("!ReadFile");
		return pmem2_lasterror_to_err();
	}

	return 0;
}

/*
 * mcsafe_op_reg_write -- safe regular write operation
 */
static int
mcsafe_op_reg_write(struct pmem2_source *src, void *buf, size_t size,
		size_t offset)
{
	HANDLE fh;
	int ret = pmem2_source_get_handle(src, &fh);
	ASSERTeq(ret, 0);

	ret = WriteFile(fh, buf, (DWORD)size, NULL, NULL);
	if (ret == 0) {
		ERR("!WriteFile");
		return pmem2_lasterror_to_err();
	}

	return 0;
}

/*
 * mcsafe_op -- machine safe operation definition
 */
typedef int (*mcsafe_op)(struct pmem2_source *src, void *buf, size_t size,
			size_t offset);

/*
 * mcsafe_ops -- array of mcsafe_op function pointers with max mcsafe operation
 *               type and max pmem2 file type as its dimensions
 */
static mcsafe_op mcsafe_ops[MAX_PMEM2_FILE_TYPE][MAX_MCSAFE_OP] = {
	[PMEM2_FTYPE_REG] = {mcsafe_op_reg_read, mcsafe_op_reg_write},
	[PMEM2_FTYPE_DEVDAX] = {mcsafe_op_none, mcsafe_op_none},
	[PMEM2_FTYPE_DIR] = {mcsafe_op_none, mcsafe_op_none},
};

/*
 * pmem2_source_type_check_mcsafe_supp -- check if source type is supports
 *                                        mcsafe operations
 */
static int
pmem2_source_type_check_mcsafe_supp(struct pmem2_source *src)
{
	if (src->type != PMEM2_SOURCE_FD && src->type != PMEM2_SOURCE_HANDLE) {
		ERR("operation doesn't support provided source type, only "\
			"sources created from file descriptor or file handle "\
			"are supported");
		return PMEM2_E_SOURCE_TYPE_NOT_SUPPORTED;
	}

	return 0;
}

/*
 * pmem2_source_check_op_size -- check if mcsafe op size doesn't go beyond
 *                               source length
 */
static int
pmem2_source_check_op_size(struct pmem2_source *src, size_t size, size_t offset)
{
	size_t src_size;
	int ret = pmem2_source_size(src, &src_size);
	if (ret)
		return ret;

	size_t max_size = (size_t)(src_size - offset);
	if (size > max_size) {
		ERR("size of read %zu from offset %zu goes beyond the file "
				"length %zu", size, offset, max_size);
		return PMEM2_E_LENGTH_OUT_OF_RANGE;
	}

	return 0;
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

	int ret = pmem2_source_type_check_mcsafe_supp(src);
	if (ret)
		return ret;

	ret = pmem2_source_check_op_size(src, size, offset);
	if (ret)
		return ret;

	enum pmem2_file_type ftype = src->value.ftype;
	ASSERT(ftype > 0 && ftype < MAX_PMEM2_FILE_TYPE);

	/* devdax on windows is not possible */
	ASSERTne(ftype, PMEM2_FTYPE_DEVDAX);

	/* source from directory file can't be created in pmem2 */
	ASSERTne(ftype, PMEM2_FTYPE_DIR);

	return mcsafe_ops[ftype][MCSAFE_OP_READ](src, buf, size, offset);
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

	int ret = pmem2_source_type_check_mcsafe_supp(src);
	if (ret)
		return ret;

	ret = pmem2_source_check_op_size(src, size, offset);
	if (ret)
		return ret;

	enum pmem2_file_type ftype = src->value.ftype;
	ASSERT(ftype > 0 && ftype < MAX_PMEM2_FILE_TYPE);

	/* devdax on windows is not possible */
	ASSERTne(ftype, PMEM2_FTYPE_DEVDAX);

	/* source from directory file can't be created in pmem2 */
	ASSERTne(ftype, PMEM2_FTYPE_DIR);

	return mcsafe_ops[ftype][MCSAFE_OP_WRITE](src, buf, size, offset);
}

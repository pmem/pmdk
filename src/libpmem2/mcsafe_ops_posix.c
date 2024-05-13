// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2024, Intel Corporation */

#include <errno.h>
#include <setjmp.h>
#include <signal.h>

#include "libpmem2.h"
#include "out.h"
#include "pmem2_utils.h"
#include "source.h"

enum mcsafe_op_type {
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
	/* suppress unused parameters */
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
	int fd;
	pmem2_source_get_fd(src, &fd);
	ASSERT(fd >= 0);

	ssize_t retsize = pread(fd, buf, size, (off_t)offset);
	if (retsize == -1) {
		if (errno == EIO) {
			ERR_WO_ERRNO(
				"physical I/O error occurred on read operation, possible bad block");
			return PMEM2_E_IO_FAIL;
		}

		ERR_W_ERRNO("pread");
		return PMEM2_E_ERRNO;
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
	int fd;
	pmem2_source_get_fd(src, &fd);
	ASSERT(fd >= 0);

	ssize_t retsize = pwrite(fd, buf, size, (off_t)offset);
	if (retsize == -1) {
		if (errno == EIO) {
			ERR_WO_ERRNO(
				"physical I/O error occurred on write operation, possible bad block");
			return PMEM2_E_IO_FAIL;
		}

		ERR_W_ERRNO("pwrite");
		return PMEM2_E_ERRNO;
	}

	return 0;
}

static __thread sigjmp_buf *Mcsafe_jmp;

/*
 * signal_handler -- called on SIGBUS
 */
static void
signal_handler(int sig)
{
	/* only SIGBUS signal should be handled */
	if (sig == SIGBUS && Mcsafe_jmp != NULL) {
		siglongjmp(*Mcsafe_jmp, 1);
	} else if (sig == SIGBUS) {
		;
	} else {
		ASSERT(0);
	}
}

/*
 * mcsafe_op -- machine safe operation definition
 */
typedef int (*mcsafe_op)(struct pmem2_source *src, void *buf, size_t size,
			size_t offset);

/*
 * handle_sigbus_execute_mcsafe_op -- execute provided operation and handle
 *                                    SIGBUS if necessary
 */
static int
handle_sigbus_execute_mcsafe_op(struct pmem2_source *src, void *buf,
		size_t size, size_t offset, mcsafe_op mcsafe_op)
{
	int ret;

	struct sigaction custom_act;
	sigemptyset(&custom_act.sa_mask);
	custom_act.sa_flags = 0;
	custom_act.sa_handler = signal_handler;

	struct sigaction old_act;
	/* register a custom signal handler */
	if (sigaction(SIGBUS, &custom_act, &old_act) == -1) {
		ERR_W_ERRNO("sigaction");
		return PMEM2_E_ERRNO;
	}

	sigjmp_buf mcsafe_jmp_buf;

	/* sigsetjmp returns nonzero only when returning from siglongjmp */
	if (sigsetjmp(mcsafe_jmp_buf, 1)) {
		ERR_WO_ERRNO("physical I/O error occurred, possible bad block");
		ret = PMEM2_E_IO_FAIL;
		goto clnup_null_global_jmp;
	}

	/* set the global jmp pointer to the jmp on stack */
	Mcsafe_jmp = &mcsafe_jmp_buf;

	ret = mcsafe_op(src, buf, size, offset);

clnup_null_global_jmp:
	Mcsafe_jmp = NULL;

	/* restore the previous signal handler */
	if (sigaction(SIGBUS, &old_act, NULL) == -1) {
		ERR_W_ERRNO("sigaction");
		return PMEM2_E_ERRNO;
	}

	return ret;
}

/*
 * devdax_read -- devdax read operation
 */
static int
devdax_read(struct pmem2_source *src, void *buf, size_t size, size_t offset)
{
	int ret;
	struct pmem2_config *cfg;
	struct pmem2_map *map;

	ret = pmem2_config_new(&cfg);
	if (ret)
		return ret;

	ret = pmem2_config_set_required_store_granularity(cfg,
			PMEM2_GRANULARITY_PAGE);
	if (ret)
		goto clnup_cfg_delete;

	ret = pmem2_map_new(&map, cfg, src);
	if (ret)
		goto clnup_cfg_delete;
	ASSERTne(map, NULL);

	void *addr = pmem2_map_get_address(map);
	pmem2_memcpy_fn memcpy_fn = pmem2_get_memcpy_fn(map);

	memcpy_fn(buf, ADDR_SUM(addr, offset), size, 0);

#ifdef DEBUG /* variables required for ASSERTs below */
	int clnup_ret =
#endif
	pmem2_map_delete(&map);
	ASSERTeq(clnup_ret, 0);
clnup_cfg_delete:
	pmem2_config_delete(&cfg);

	return ret;
}

/*
 * devdax_write -- devdax write operation
 */
static int
devdax_write(struct pmem2_source *src, void *buf, size_t size, size_t offset)
{
	int ret;
	struct pmem2_config *cfg;
	struct pmem2_map *map;

	ret = pmem2_config_new(&cfg);
	if (ret)
		return ret;

	ret = pmem2_config_set_required_store_granularity(cfg,
			PMEM2_GRANULARITY_PAGE);
	if (ret)
		goto clnup_cfg_delete;

	ret = pmem2_map_new(&map, cfg, src);
	if (ret)
		goto clnup_cfg_delete;
	ASSERTne(map, NULL);

	void *addr = pmem2_map_get_address(map);
	pmem2_memcpy_fn memcpy_fn = pmem2_get_memcpy_fn(map);

	memcpy_fn(ADDR_SUM(addr, offset), buf, size, 0);

#ifdef DEBUG /* variables required for ASSERTs below */
	int clnup_ret =
#endif
	pmem2_map_delete(&map);
	ASSERTeq(clnup_ret, 0);
clnup_cfg_delete:
	pmem2_config_delete(&cfg);

	return ret;
}

/*
 * mcsafe_op_devdax_read -- safe devdax read operation
 */
static int
mcsafe_op_devdax_read(struct pmem2_source *src, void *buf, size_t size,
		size_t offset)
{
	return handle_sigbus_execute_mcsafe_op(src, buf, size, offset,
			devdax_read);
}

/*
 * mcsafe_op_devdax_write -- safe devdax write operation
 */
static int
mcsafe_op_devdax_write(struct pmem2_source *src, void *buf, size_t size,
		size_t offset)
{
	return handle_sigbus_execute_mcsafe_op(src, buf, size, offset,
			devdax_write);
}

/*
 * mcsafe_ops -- array of mcsafe_op function pointers with max mcsafe operation
 *               type and max pmem2 file type as its dimensions
 */
static mcsafe_op mcsafe_ops[MAX_PMEM2_FILE_TYPE][MAX_MCSAFE_OP] = {
	[PMEM2_FTYPE_REG] = {mcsafe_op_reg_read, mcsafe_op_reg_write},
	[PMEM2_FTYPE_DEVDAX] = {mcsafe_op_devdax_read, mcsafe_op_devdax_write},
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
		ERR_WO_ERRNO(
			"operation doesn't support provided source type, only sources created from file descriptor or file handle are supported");
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
		ERR_WO_ERRNO(
			"size of read %zu from offset %zu goes beyond the file length %zu",
			size, offset, max_size);
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

	/* source from directory file can't be created in pmem2 */
	ASSERTne(ftype, PMEM2_FTYPE_DIR);

	return mcsafe_ops[ftype][MCSAFE_OP_WRITE](src, buf, size, offset);
}

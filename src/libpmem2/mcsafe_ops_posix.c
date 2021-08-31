// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021, Intel Corporation */

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
 * mcsafe_op_write -- wrapper for write operation
 *                    (incompatible pointer types from const modifier)
 */
static ssize_t
mcsafe_op_write(int fd, void *buf, size_t size, off_t offset)
{
	return pwrite(fd, buf, size, offset);
}

/*
 * mcsafe_op_devdax_read -- reads provided size of bytes from an address to
 *                          a buffer (devdax operation)
 */
static void *
mcsafe_op_devdax_read(void *addr, void *buf, size_t size, unsigned flags,
		pmem2_memcpy_fn memcpy_fn)
{
	return memcpy_fn(buf, addr, size, flags);
}

/*
 * mcsafe_op_devdax_write -- writes provided size of bytes from a buffer to
 *                           an address (devdax operation)
 */
static void *
mcsafe_op_devdax_write(void *addr, void *buf, size_t size, unsigned flags,
		pmem2_memcpy_fn memcpy_fn)
{
	return memcpy_fn(addr, buf, size, flags);
}

#define MCSAFE_OP_MAX_STR 16

static const struct {
	ssize_t (*op)(int fd, void *buf, size_t size, off_t offset);
	char op_str[MCSAFE_OP_MAX_STR];
	char op_err_str[MCSAFE_OP_MAX_STR];
	void *(*op_devdax)(void *addr, void *buf, size_t size, unsigned flags,
			pmem2_memcpy_fn memcpy_fn);
} mcsafe_op_args[MAX_MCSAFE_OP] = {
	[MCSAFE_OP_READ] = {
		.op = pread,
		.op_str = "read",
		.op_err_str = "!pread",
		.op_devdax = mcsafe_op_devdax_read,
	},

	[MCSAFE_OP_WRITE] = {
		.op = mcsafe_op_write,
		.op_str = "write",
		.op_err_str = "!pwrite",
		.op_devdax = mcsafe_op_devdax_write
	}
};

static __thread sigjmp_buf *Mcsafe_jmp;

/*
 * signal_handler -- called on SIGBUS
 */
static void
signal_handler(int sig)
{
	/* only SIGBUS signal should be handled */
	if (sig == SIGBUS && Mcsafe_jmp != NULL)
		siglongjmp(*Mcsafe_jmp, 1);
	else
		ASSERT(0);
}

/*
 * pmem2_source_mcsafe_operation -- execute operation in a safe manner
 *                                  (detect badblocks)
 */
static int
pmem2_source_mcsafe_operation(struct pmem2_source *src, void *buf, size_t size,
		size_t offset, enum mcsafe_op_type op_type)
{
	ASSERT(op_type >= 0 && op_type < MAX_MCSAFE_OP);

	if (src->type != PMEM2_SOURCE_FD && src->type != PMEM2_SOURCE_HANDLE) {
		ERR("operation doesn't support provided source type, only "\
			"sources created from file descriptor or file handle "\
			"are supported");
		return PMEM2_E_SOURCE_TYPE_NOT_SUPPORTED;
	}

	int ret;
	int clnup_ret;
	int fd;
	struct pmem2_config *cfg;
	struct pmem2_map *map;

	ret = pmem2_source_get_fd(src, &fd);
	ASSERT(fd >= 0);

	size_t file_size;
	ret = pmem2_source_size(src, &file_size);
	if (ret)
		return ret;

	size_t max_size = (size_t)(file_size - offset);
	if (size > max_size) {
		ERR("size of %s %zu from offset %zu goes beyond the file " \
			"length %zu", mcsafe_op_args[op_type].op_str, size,
			offset, max_size);
		return PMEM2_E_LENGTH_OUT_OF_RANGE;
	}

	enum pmem2_file_type ftype = src->value.ftype;

	ASSERT(ftype > 0 && ftype < MAX_PMEM2_FILE_TYPE);

	/* source from directory file can't be created in pmem2 */
	ASSERTne(ftype, PMEM2_FTYPE_DIR);

	/* fsdax read/write relies on pread/pwrite */
	if (ftype == PMEM2_FTYPE_REG) {
		ssize_t retsize = mcsafe_op_args[op_type].op(fd, buf, size,
				(off_t)offset);
		if (retsize == -1) {
			if (errno == EIO) {
				ERR("physical I/O error occured, " \
					"possible bad block");
				return PMEM2_E_IO_FAIL;
			}

			ERR("%s", mcsafe_op_args[op_type].op_err_str);
			return PMEM2_E_ERRNO;
		}

		return 0;
	}

	/* devdax requires a mapping and sigbus handler (badblock error) */
	ret = pmem2_config_new(&cfg);
	if (ret)
		return ret;

	ret = pmem2_config_set_required_store_granularity(cfg,
			PMEM2_GRANULARITY_PAGE);
	if (ret)
		goto err_cfg_delete;

	ret = pmem2_map_new(&map, cfg, src);
	if (ret)
		goto err_cfg_delete;

	void *addr = pmem2_map_get_address(map);
	pmem2_memcpy_fn memcpy_fn = pmem2_get_memcpy_fn(map);

	struct sigaction act;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	act.sa_handler = signal_handler;

	struct sigaction cur_act;
	/* register a custom signal handler */
	ret = sigaction(SIGBUS, &act, &cur_act);
	if (ret == -1) {
		ERR("!sigaction");
		ret = PMEM2_E_ERRNO;
		goto err_cfg_delete;
	}

	sigjmp_buf mcsafe_jmp_buf;

	/* sigsetjmp returns nonzero only when returning from siglongjmp */
	if (sigsetjmp(mcsafe_jmp_buf, 1)) {
		Mcsafe_jmp = NULL;

		clnup_ret = sigaction(SIGBUS, &cur_act, NULL);
		ASSERTeq(clnup_ret, 0);

		ret = pmem2_map_delete(&map);
		ASSERTeq(ret, 0);
		ret = pmem2_config_delete(&cfg);
		ASSERTeq(ret, 0);

		ERR("physical I/O error occured, possible bad block");
		return PMEM2_E_IO_FAIL;
	}

	/* set the global jmp pointer to the jmp on stack */
	Mcsafe_jmp = &mcsafe_jmp_buf;

	mcsafe_op_args[op_type].op_devdax(ADDR_SUM(addr, offset), buf, size, 0,
			memcpy_fn);

	Mcsafe_jmp = NULL;

	/* restore the previous signal handler */
	ret = sigaction(SIGBUS, &cur_act, NULL);
	if (ret == -1) {
		ERR("!sigaction");
		ret = PMEM2_E_ERRNO;
		goto err_cfg_delete;
	}

	ret = pmem2_map_delete(&map);
err_cfg_delete:
	clnup_ret = pmem2_config_delete(&cfg);
	ASSERTeq(clnup_ret, 0);
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

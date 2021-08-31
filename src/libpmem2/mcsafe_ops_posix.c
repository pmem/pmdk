// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021, Intel Corporation */

#include <errno.h>
#include <setjmp.h>

#include "libpmem2.h"
#include "out.h"
#include "pmem2_utils.h"
#include "source.h"

enum mcsafe_op {
	MCSAFE_OP_READ,
	MCSAFE_OP_WRITE,

	MAX_MCSAFE_OP,
};

static sigjmp_buf *Mcsafe_jmp;

/*
 * signal_handler -- called on SIGBUS
 */
static void
signal_handler(int sig)
{
	siglongjmp(*Mcsafe_jmp, 1);
}

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

	/* devdax requires a mapping and sigbus handler (badblock error) */
	struct pmem2_config *cfg;
	ret = pmem2_config_new(&cfg);
	if (ret)
		return ret;

	ret = pmem2_config_set_required_store_granularity(cfg,
			PMEM2_GRANULARITY_PAGE);
	if (ret)
		goto err_cfg_delete;

	struct pmem2_map *map;
	ret = pmem2_map_new(&map, cfg, src);
	if (ret)
		goto err_cfg_delete;

	void *addr = pmem2_map_get_address(map);

	struct sigaction act;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	act.sa_handler = signal_handler;

	struct sigaction cur_act;
	/* register a custom signal handler */
	sigaction(SIGBUS, &act, &cur_act);

	/* set the global jmp pointer to the jmp on stack */
	sigjmp_buf mcsafe_jmp_buf;
	Mcsafe_jmp = &mcsafe_jmp_buf;

	/* sigsetjmp returns nonzero only when returning from siglongjmp */
	if (sigsetjmp(*Mcsafe_jmp, 1)) {
		sigaction(SIGBUS, &cur_act, NULL);

		pmem2_map_delete(&map);
		pmem2_config_delete(&cfg);

		ERR("physical I/O error occured, possible bad block");
		return PMEM2_E_IO_FAIL;
	}

	if (op_type == MCSAFE_OP_READ)
		memcpy(buf, ADDR_SUM(addr, offset), size);
	else if (op_type == MCSAFE_OP_WRITE)
		memcpy(ADDR_SUM(addr, offset), buf, size);

	/* restore the previous signal handler */
	sigaction(SIGBUS, &cur_act, NULL);

	ret = pmem2_map_delete(&map);

err_cfg_delete:
	pmem2_config_delete(&cfg);
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

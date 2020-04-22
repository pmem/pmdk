// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019-2020, Intel Corporation */

/*
 * mocks_other.c -- mocked various functions
 *                  used in pmem2_badblock_mocks.c
 */

#include <sys/stat.h>

#include "unittest.h"
#include "out.h"
#include "pmem2_badblock_mocks.h"
#include "extent.h"

/*
 * fstat - mock fstat
 *
 *   0 <= fd <= 100 - regular file, did not found any matching device
 * 101 <= fd <= 200 - regular file
 * 201 <= fd <= 300 - character device
 * 301 <= fd <= 400 - directory
 * 401 <= fd        - block device
 *
 * fd % 100 == 0  - did not found any matching device
 * fd % 100 <  50 - namespace mode
 * fd % 100 >= 50 - region mode
 */
FUNC_MOCK(fstat, int, int fd, struct stat *buf)
FUNC_MOCK_RUN_DEFAULT {
	ASSERTne(buf, NULL);

	memset(buf, 0, sizeof(struct stat));

	/* default: regular file */
	buf->st_mode = __S_IFREG;

	/* default block size */
	buf->st_blksize = BLK_SIZE_1KB;

	/* default: did not found any matching device */
	buf->st_ino = MODE_NO_DEV;

	if (fd < FD_REG_FILE)
		/* regular file, did not found any matching device */
		return 0;

	/*
	 * st->st_ino ==  0 - did not found any matching device
	 * st->st_ino <  50 - namespace mode
	 * st->st_ino >= 50 - region mode
	 */
	buf->st_ino = (__ino_t)(fd % MASK_MODE);

	if (fd < FD_CHR_DEV) {
		/* regular file */
		return 0;
	}

	if (fd < FD_DIRECTORY) {
		/* character device */
		buf->st_mode = __S_IFCHR;
		return 0;
	}

	if (fd < FD_BLK_DEV) {
		/* directory */
		buf->st_mode = __S_IFDIR;
		return 0;
	}

	/* block device */
	buf->st_mode = __S_IFBLK;

	return 0;
}
FUNC_MOCK_END

/*
 * pmem2_extents_create_get -- allocate extents structure and get extents
 *                             of the given file
 */
FUNC_MOCK(pmem2_extents_create_get, int,
		int fd, struct extents **exts)
FUNC_MOCK_RUN_DEFAULT {
	return get_extents(fd, exts);
}
FUNC_MOCK_END

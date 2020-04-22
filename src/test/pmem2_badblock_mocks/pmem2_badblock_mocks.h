// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019-2020, Intel Corporation */

/*
 * pmem2_badblock_mocks.h -- definitions for pmem2_badblock_mocks test
 */

#include <ndctl/libndctl.h>
#include "extent.h"

/*
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

#define FD_REG_FILE	100	/* regular file */
#define FD_CHR_DEV	200	/* character device */
#define FD_DIRECTORY	300	/* directory */
#define FD_BLK_DEV	400	/* block device */

#define MODE_NO_DEV	0	/* did not found any matching device */
#define MODE_NAMESPACE	1	/* namespace mode */
#define MODE_REGION	51	/* region mode */

#define MASK_MODE	100	/* mode "mask" */
#define MASK_SET	50	/* set "mask" */

#define BLK_SIZE_1KB	1024	/* default block size */
#define DEV_SIZE_1GB	(1024 * 1024 * 1024) /* default block size - 1 GiB */

/* Numbers of tests */

#define TEST_BB_REG_NAMESPACE_0	(FD_REG_FILE + MODE_NAMESPACE + 0) // 101
#define TEST_BB_REG_NAMESPACE_1	(FD_REG_FILE + MODE_NAMESPACE + 1) // 102
#define TEST_BB_REG_NAMESPACE_2	(FD_REG_FILE + MODE_NAMESPACE + 2) // 103
#define TEST_BB_REG_NAMESPACE_3	(FD_REG_FILE + MODE_NAMESPACE + 3) // 104

#define TEST_BB_REG_REGION_0	(FD_REG_FILE + MODE_REGION + 0) // 151
#define TEST_BB_REG_REGION_1	(FD_REG_FILE + MODE_REGION + 1) // 152
#define TEST_BB_REG_REGION_2	(FD_REG_FILE + MODE_REGION + 2) // 153
#define TEST_BB_REG_REGION_3	(FD_REG_FILE + MODE_REGION + 3) // 154

#define TEST_BB_CHR_REGION_0	(FD_CHR_DEV + MODE_REGION + 0) // 251
#define TEST_BB_CHR_REGION_1	(FD_CHR_DEV + MODE_REGION + 1) // 252
#define TEST_BB_CHR_REGION_2	(FD_CHR_DEV + MODE_REGION + 2) // 253
#define TEST_BB_CHR_REGION_3	(FD_CHR_DEV + MODE_REGION + 3) // 254

extern struct badblock *get_next_hw_badblock(unsigned test, unsigned *i_bb);
extern int get_extents(int fd, struct extents **exts);

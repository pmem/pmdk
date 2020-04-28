// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019-2020, Intel Corporation */

/*
 * pmem2_badblock_mocks.h -- definitions for pmem2_badblock_mocks test
 */

#include <ndctl/libndctl.h>
#include "extent.h"

/* fd bits 6-8: type of device */
#define FD_REG_FILE	(1 << 6)	/* regular file */
#define FD_CHR_DEV	(2 << 6)	/* character device */
#define FD_DIRECTORY	(3 << 6)	/* directory */
#define FD_BLK_DEV	(4 << 6)	/* block device */

/* fd bits 4-5: ndctl mode */
#define MODE_NO_DEVICE	(1 << 4)	/* did not found any matching device */
#define MODE_NAMESPACE	(2 << 4)	/* namespace mode */
#define MODE_REGION	(3 << 4)	/* region mode */

/* fd bits 0-3: number of test */

/* masks */
#define MASK_DEVICE	0b0111000000	/* bits 6-8: device mask */
#define MASK_MODE	0b0000110000	/* bits 4-5: mode mask */
#define MASK_TEST	0b0000001111	/* bits 0-3: test mask */

/* checks */
#define IS_MODE_NO_DEVICE(x)	((x & MASK_MODE) == MODE_NO_DEVICE)
#define IS_MODE_NAMESPACE(x)	((x & MASK_MODE) == MODE_NAMESPACE)
#define IS_MODE_REGION(x)	((x & MASK_MODE) == MODE_REGION)

/* sizes */
#define BLK_SIZE_1KB	1024	/* default block size - 1kB */
#define DEV_SIZE_1GB	(1024 * 1024 * 1024) /* default block size - 1 GiB */

extern struct badblock *get_nth_hw_badblock(unsigned test, unsigned *i_bb);
extern int get_extents(int fd, struct extents **exts);

// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2018-2020, Intel Corporation */

/*
 * usc_permission_check.c -- checks whether it's possible to read usc
 *			     with current permissions
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <libpmem2.h>
#include "os.h"

/*
 * This program returns:
 * - 0 when usc can be read with current permissions
 * - 1 when permissions are not sufficient
 * - 2 when other error occurs
 */
int
main(int argc, char *argv[])
{
	if (argc != 2) {
		fprintf(stderr, "usage: %s filename\n", argv[0]);
		return 2;
	}

	uint64_t usc;
	struct pmem2_config *cfg;
	if (pmem2_config_new(&cfg))
		fprintf(stderr, "pmem2_config_new: %s\n", pmem2_errormsg());

	int fd = os_open(argv[0], O_RDONLY);
	if (fd < 0)
		perror("open");

	if (pmem2_config_set_fd(cfg, fd))
		fprintf(stderr, "pmem2_config_set_fd: %s\n", pmem2_errormsg());

	int ret = pmem2_get_device_usc(cfg, &usc);

	if (ret == 0)
		return 0;
	else if (errno == -EACCES)
		return 1;
	else
		return 2;
}

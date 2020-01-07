// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2018-2019, Intel Corporation */

/*
 * usc_permission_check.c -- checks whether it's possible to read usc
 *			     with current permissions
 */

#include <errno.h>
#include <stdio.h>
#include "os_dimm.h"

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
	int ret = os_dimm_usc(argv[1], &usc);

	if (ret == 0)
		return 0;
	else if (errno == EACCES)
		return 1;
	else
		return 2;
}

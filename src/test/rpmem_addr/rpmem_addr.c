// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016-2017, Intel Corporation */

/*
 * rpmem_addr.c -- unit test for parsing target address
 */

#include "unittest.h"
#include "rpmem_common.h"

int
main(int argc, char *argv[])
{
	START(argc, argv, "rpmem_addr");

	struct rpmem_target_info *info;

	for (int i = 1; i < argc; i++) {
		info = rpmem_target_parse(argv[i]);
		if (info) {
			UT_OUT("'%s': '%s' '%s' '%s'", argv[i],
				info->flags & RPMEM_HAS_USER ?
					info->user : "(null)",
				*info->node ? info->node : "(null)",
				info->flags & RPMEM_HAS_SERVICE ?
					info->service : "(null)");
			free(info);
		} else {
			UT_OUT("!%s", argv[i]);
		}
	}

	DONE(NULL);
}

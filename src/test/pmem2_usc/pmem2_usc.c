// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021, Intel Corporation */

/*
 * pmem2_usc.c -- pmem2_usc unittests
 */

#include "libpmem2.h"
#include "unittest.h"
#include "out.h"

int
main(int argc, char **argv)
{
	START(argc, argv, "pmem2_usc");

	char *file = argv[1];
	int fd = OPEN(file, O_RDWR);

	struct pmem2_source *src;
	int ret = pmem2_source_from_fd(&src, fd);
	UT_ASSERTeq(ret, 0);

	uint64_t usc;
	ret = pmem2_source_device_usc(src, &usc);
	UT_ASSERTeq(ret, 0);

	UT_OUT("USC: %lu", usc);

	CLOSE(fd);

	DONE(NULL);
}

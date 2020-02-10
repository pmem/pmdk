// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

#define INVALID_FD (-1)

struct pmem2_source {
	/* a source file descriptor / handle for the designed mapping */
#ifdef _WIN32
	HANDLE handle;
#else
	int fd;
#endif
};

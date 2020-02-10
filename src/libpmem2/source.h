// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

#ifndef PMEM2_SOURCE_H
#define PMEM2_SOURCE_H 1

#define INVALID_FD (-1)

struct pmem2_source {
	/* a source file descriptor / handle for the designed mapping */
#ifdef _WIN32
	HANDLE handle;
#else
	int fd;
#endif
};

#endif /* PMEM2_SOURCE_H */

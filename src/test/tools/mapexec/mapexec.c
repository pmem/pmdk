// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/*
 * mapexec.c -- run mmap with exec
 *
 * Return values:
 *	1 - exec allowed
 *	0 - exec not allowed
 */

#include <stdio.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>

#include "os.h"

#define PAGE_SIZE 4096

int
main(int argc, char *argv[])
{
	int ret = 1;
	int fd = os_open(argv[1], O_RDWR);
	if (fd < 0) {
		perror("cannot open file");
		return 0;
	}

	void *map = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE | PROT_EXEC,
			MAP_SHARED, fd, 0);
	if (map == MAP_FAILED && errno == EPERM)
		ret = 0;
	if (map == MAP_FAILED)
		goto end;

	munmap(map, PAGE_SIZE);
end:
	os_close(fd);
	return ret;
}

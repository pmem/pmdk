// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2014-2019, Intel Corporation */

/*
 * manpage.c -- simple example for the libpmemblk man page
 */

#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#ifndef _WIN32
#include <unistd.h>
#endif
#include <string.h>
#include <libpmemblk.h>

/* size of the pmemblk pool -- 1 GB */
#define POOL_SIZE ((size_t)(1 << 30))

/* size of each element in the pmem pool */
#define ELEMENT_SIZE 1024

int
main(int argc, char *argv[])
{
	const char path[] = "/pmem-fs/myfile";
	PMEMblkpool *pbp;
	size_t nelements;
	char buf[ELEMENT_SIZE];

	/* create the pmemblk pool or open it if it already exists */
	pbp = pmemblk_create(path, ELEMENT_SIZE, POOL_SIZE, 0666);

	if (pbp == NULL)
	    pbp = pmemblk_open(path, ELEMENT_SIZE);

	if (pbp == NULL) {
		perror(path);
		exit(1);
	}

	/* how many elements fit into the file? */
	nelements = pmemblk_nblock(pbp);
	printf("file holds %zu elements\n", nelements);

	/* store a block at index 5 */
	strcpy(buf, "hello, world");
	if (pmemblk_write(pbp, buf, 5) < 0) {
		perror("pmemblk_write");
		exit(1);
	}

	/* read the block at index 10 (reads as zeros initially) */
	if (pmemblk_read(pbp, buf, 10) < 0) {
		perror("pmemblk_read");
		exit(1);
	}

	/* zero out the block at index 5 */
	if (pmemblk_set_zero(pbp, 5) < 0) {
		perror("pmemblk_set_zero");
		exit(1);
	}

	/* ... */

	pmemblk_close(pbp);
	return 0;
}

// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2014-2017, Intel Corporation */

/*
 * full_copy.c -- show how to use pmem_memcpy_nodrain()
 *
 * usage: full_copy src-file dst-file
 *
 * Copies src-file to dst-file in 4k chunks.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#ifndef _WIN32
#include <unistd.h>
#else
#include <io.h>
#endif
#include <string.h>
#include <libpmem.h>

/* copying 4k at a time to pmem for this example */
#define BUF_LEN 4096

/*
 * do_copy_to_pmem -- copy to pmem, postponing drain step until the end
 */
static void
do_copy_to_pmem(char *pmemaddr, int srcfd, off_t len)
{
	char buf[BUF_LEN];
	int cc;

	/* copy the file, saving the last flush step to the end */
	while ((cc = read(srcfd, buf, BUF_LEN)) > 0) {
		pmem_memcpy_nodrain(pmemaddr, buf, cc);
		pmemaddr += cc;
	}

	if (cc < 0) {
		perror("read");
		exit(1);
	}

	/* perform final flush step */
	pmem_drain();
}

/*
 * do_copy_to_non_pmem -- copy to a non-pmem memory mapped file
 */
static void
do_copy_to_non_pmem(char *addr, int srcfd, off_t len)
{
	char *startaddr = addr;
	char buf[BUF_LEN];
	int cc;

	/* copy the file, saving the last flush step to the end */
	while ((cc = read(srcfd, buf, BUF_LEN)) > 0) {
		memcpy(addr, buf, cc);
		addr += cc;
	}

	if (cc < 0) {
		perror("read");
		exit(1);
	}

	/* flush it */
	if (pmem_msync(startaddr, len) < 0) {
		perror("pmem_msync");
		exit(1);
	}
}

int
main(int argc, char *argv[])
{
	int srcfd;
	struct stat stbuf;
	char *pmemaddr;
	size_t mapped_len;
	int is_pmem;

	if (argc != 3) {
		fprintf(stderr, "usage: %s src-file dst-file\n", argv[0]);
		exit(1);
	}

	/* open src-file */
	if ((srcfd = open(argv[1], O_RDONLY)) < 0) {
		perror(argv[1]);
		exit(1);
	}

	/* find the size of the src-file */
	if (fstat(srcfd, &stbuf) < 0) {
		perror("fstat");
		exit(1);
	}

	/* create a pmem file and memory map it */
	if ((pmemaddr = pmem_map_file(argv[2], stbuf.st_size,
				PMEM_FILE_CREATE|PMEM_FILE_EXCL,
				0666, &mapped_len, &is_pmem)) == NULL) {
		perror("pmem_map_file");
		exit(1);
	}

	/* determine if range is true pmem, call appropriate copy routine */
	if (is_pmem)
		do_copy_to_pmem(pmemaddr, srcfd, stbuf.st_size);
	else
		do_copy_to_non_pmem(pmemaddr, srcfd, stbuf.st_size);

	close(srcfd);
	pmem_unmap(pmemaddr, mapped_len);

	exit(0);
}

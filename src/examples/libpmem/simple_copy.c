// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2014-2017, Intel Corporation */

/*
 * simple_copy.c -- show how to use pmem_memcpy_persist()
 *
 * usage: simple_copy src-file dst-file
 *
 * Reads 4k from src-file and writes it to dst-file.
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

/* just copying 4k to pmem for this example */
#define BUF_LEN 4096

int
main(int argc, char *argv[])
{
	int srcfd;
	char buf[BUF_LEN];
	char *pmemaddr;
	size_t mapped_len;
	int is_pmem;
	int cc;

	if (argc != 3) {
		fprintf(stderr, "usage: %s src-file dst-file\n", argv[0]);
		exit(1);
	}

	/* open src-file */
	if ((srcfd = open(argv[1], O_RDONLY)) < 0) {
		perror(argv[1]);
		exit(1);
	}

	/* create a pmem file and memory map it */
	if ((pmemaddr = pmem_map_file(argv[2], BUF_LEN,
				PMEM_FILE_CREATE|PMEM_FILE_EXCL,
				0666, &mapped_len, &is_pmem)) == NULL) {
		perror("pmem_map_file");
		exit(1);
	}

	/* read up to BUF_LEN from srcfd */
	if ((cc = read(srcfd, buf, BUF_LEN)) < 0) {
		pmem_unmap(pmemaddr, mapped_len);
		perror("read");
		exit(1);
	}

	/* write it to the pmem */
	if (is_pmem) {
		pmem_memcpy_persist(pmemaddr, buf, cc);
	} else {
		memcpy(pmemaddr, buf, cc);
		pmem_msync(pmemaddr, cc);
	}

	close(srcfd);
	pmem_unmap(pmemaddr, mapped_len);

	exit(0);
}

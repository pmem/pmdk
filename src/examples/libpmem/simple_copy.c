/*
 * Copyright 2014-2016, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *
 *     * Neither the name of the copyright holder nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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
#include <unistd.h>
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

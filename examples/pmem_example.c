/*
 * Copyright (c) 2014, Intel Corporation
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
 *     * Neither the name of Intel Corporation nor the names of its
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
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY LOG OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/mman.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <libpmem.h>

int
main(int argc, char *argv[])
{
	int fd;
	char *pmaddr;

	/* memory map some persistent memory */
	if ((fd = open("/my/pmem-aware/fs/myfile", O_RDWR)) < 0) {
		perror("open");
		exit(1);
	}

	/* just map 4k for this example */
	if ((pmaddr = mmap(NULL, 4096, PROT_READ|PROT_WRITE,
				MAP_SHARED, fd, 0)) == MAP_FAILED) {
		perror("mmap");
		exit(1);
	}
	close(fd);

	/* store a string to the persistent memory */
	strcpy(pmaddr, "hello, persistent memory");

	/*
	 * The above stores may or may not be sitting in cache at
	 * this point, depending on other system activity causing
	 * cache pressure.  Now force the change to be durable
	 * (flushed all the say to the persistent memory).  If
	 * unsure whether the file is really persistent memory,
	 * use pmem_is_pmem() to decide whether pmem_persist() can
	 * be used, or whether msync() must be used.
	 */
	if (pmem_is_pmem(pmaddr, 4096))
		pmem_persist(pmaddr, 4096, 0);
	else
		msync(pmaddr, 4096, MS_SYNC);
}

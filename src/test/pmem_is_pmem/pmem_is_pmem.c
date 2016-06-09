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
 * pmem_is_pmem.c -- unit test for pmem_is_pmem()
 *
 * usage: pmem_is_pmem file [env]
 */

#include "unittest.h"

#define NTHREAD 16

void *Addr;
size_t Size;

/*
 * worker -- the work each thread performs
 */
static void *
worker(void *arg)
{
	int *ret = (int *)arg;
	*ret =  pmem_is_pmem(Addr, Size);
	return NULL;
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "pmem_is_pmem");

	if (argc <  2 || argc > 3)
		UT_FATAL("usage: %s file [env]", argv[0]);

	if (argc == 3)
		UT_ASSERTeq(setenv("PMEM_IS_PMEM_FORCE", argv[2], 1), 0);

	int fd = OPEN(argv[1], O_RDWR);

	struct stat stbuf;
	FSTAT(fd, &stbuf);

	Size = stbuf.st_size;
	Addr = MMAP(0, stbuf.st_size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);

	CLOSE(fd);

	pthread_t threads[NTHREAD];
	int ret[NTHREAD];

	/* kick off NTHREAD threads */
	for (int i = 0; i < NTHREAD; i++)
		PTHREAD_CREATE(&threads[i], NULL, worker, &ret[i]);

	/* wait for all the threads to complete */
	for (int i = 0; i < NTHREAD; i++)
		PTHREAD_JOIN(threads[i], NULL);

	/* verify that all the threads return the same value */
	for (int i = 1; i < NTHREAD; i++)
		UT_ASSERTeq(ret[0], ret[i]);

	UT_OUT("%d", ret[0]);

	UT_ASSERTeq(unsetenv("PMEM_IS_PMEM_FORCE"), 0);

	UT_OUT("%d", pmem_is_pmem(Addr, Size));

	DONE(NULL);
}

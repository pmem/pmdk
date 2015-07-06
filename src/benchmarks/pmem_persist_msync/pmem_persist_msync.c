/*
 * Copyright (c) 2015, Intel Corporation
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

/*
 * pmem_persist_msync.c -- benchmark of pmem_persist vs. pmem_msync
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <inttypes.h>
#include <time.h>
#include <errno.h>

#include <libpmem.h>

#define	NANOSEC_IN_SEC 1000000000.0

typedef void (*persist_fn)(void *, size_t);

static double
benchmark_func(persist_fn func, uint8_t *pool,
		size_t data_size, int iterations)
{
	struct timespec time_start, time_stop;
	int i;

	/* fill pool with a random value */
	clock_gettime(CLOCK_MONOTONIC, &time_start);
	memset(pool, time_start.tv_nsec & 0xFF, data_size * iterations);

	/* measure execution time of <func> */
	clock_gettime(CLOCK_MONOTONIC, &time_start);
	for (i = 0; i < iterations; i++)
		(*func)(pool + i * data_size, data_size);
	clock_gettime(CLOCK_MONOTONIC, &time_stop);

	return ((time_stop.tv_sec - time_start.tv_sec) +
		(time_stop.tv_nsec - time_start.tv_nsec) / NANOSEC_IN_SEC)
		/ (double)iterations;
}

int
main(int argc, char *argv[])
{
	char *path;
	size_t data_size, pool_size;
	int iterations;
	int is_pmem;
	int fd;
	uint8_t *pool;
	double exec_time_pmem_persist, exec_time_pmem_msync;

	if (argc < 4) {
		printf("Usage %s <file_name> <data_size> <iterations>\n", argv[0]);
		return 0;
	}

	path = argv[1];
	data_size = atoi(argv[2]);
	iterations = atoi(argv[3]);
	pool_size = data_size * iterations;

	if ((fd = open(path, O_RDWR|O_CREAT|O_EXCL, S_IWUSR|S_IRUSR)) < 0) {
		perror("open");
		return -1;
	}

	if ((errno = posix_fallocate(fd, 0, pool_size)) != 0) {
		perror("posix_fallocate");
		goto err;
	}

	if ((pool = pmem_map(fd)) == NULL) {
		perror("pmem_map");
		goto err;
	}
	close(fd);

	/* check if range is true pmem */
	is_pmem = pmem_is_pmem(pool, pool_size);
	if (is_pmem) {
		/* benchmarking pmem_persist */
		exec_time_pmem_persist = benchmark_func(pmem_persist, pool,
						data_size, iterations);
	} else {
		fprintf(stderr, "Notice: pmem_persist is not benchmarked,"
			" because given file (%s) is not in Persistent Memory"
			" aware file system.\n", path);
		exec_time_pmem_persist = 0.0;
	}

	/* benchmarking pmem_msync */
	exec_time_pmem_msync = benchmark_func((persist_fn)pmem_msync, pool,
						data_size, iterations);

	printf("%zu;%e;%zu;%e;%zu;%e\n",
		data_size, exec_time_pmem_persist,
		data_size, exec_time_pmem_msync,
		data_size, exec_time_pmem_persist / exec_time_pmem_msync);

	return 0;

err:
	close(fd);
	unlink(path);
	return -1;
}

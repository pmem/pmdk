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
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * workers.c -- thread workers for the PMEMBLK benchmark
 */

#include <workers.h>
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#define	__USE_UNIX98
#include <unistd.h>

/*
 * r_worker -- read worker function
 */
void *
r_worker(void *arg)
{
	struct worker_info *my_info = arg;
	unsigned char buf[my_info->block_size];

	for (int i = 0; i < my_info->num_ops; i++) {
		off_t lba = rand_r(&my_info->seed) % my_info->num_blocks;

		/* read */
		if (pmemblk_read(my_info->handle, buf, lba) < 0) {
			fprintf(stderr, "!read      lba %zu", lba);
		}
	}
	return NULL;
}

/*
 * w_worker -- write worker function
 */
void *
w_worker(void *arg)
{
	struct worker_info *my_info = arg;
	unsigned char buf[my_info->block_size];
	memset(buf, 1, my_info->block_size);

	for (int i = 0; i < my_info->num_ops; i++) {
		off_t lba = rand_r(&my_info->seed) % my_info->num_blocks;

		if (pmemblk_write(my_info->handle, buf, lba) < 0) {
			fprintf(stderr, "!write     lba %zu", lba);
		}
	}
	return NULL;
}

/*
 * prep_worker -- worker for the prep mode. Writes the whole
 * calculated range of lba's. This is similar to the rf_worker.
 */
void *
prep_worker(void *arg)
{
	struct worker_info *my_info = arg;
	unsigned long long blocks_in_lane = my_info->num_blocks
			/ my_info->file_lanes;
	off_t start_lba = my_info->thread_index * blocks_in_lane;
	off_t stop_lba = (my_info->thread_index + 1) * blocks_in_lane;
	unsigned char buf[my_info->block_size];
	memset(buf, 1, my_info->block_size);

	for (off_t lba = start_lba; lba < stop_lba; ++lba) {
		if (pmemblk_write(my_info->handle, buf, lba) < 0) {
			fprintf(stderr, "!write     lba %zu", lba);
		}
	}
	return NULL;
}

/*
 * warmup_worker -- worker for the warm-up. Reads the whole
 * calculated range of lba's. This is similar to the rf_worker.
 */
void *
warmup_worker(void *arg)
{
	struct worker_info *my_info = arg;
	unsigned long long blocks_in_lane = my_info->num_blocks
			/ my_info->file_lanes;
	off_t start_lba = my_info->thread_index * blocks_in_lane;
	off_t stop_lba = (my_info->thread_index + 1) * blocks_in_lane;
	unsigned char buf[my_info->block_size];

	for (off_t lba = start_lba; lba < stop_lba; ++lba) {
		if (pmemblk_read(my_info->handle, buf, lba) < 0) {
			fprintf(stderr, "!read     lba %zu", lba);
		}
	}
	return NULL;
}

/*
 * worker -- read worker function for file io
 */
void *
rf_worker(void *arg)
{
	struct worker_info *my_info = arg;
	unsigned char buf[my_info->block_size];
	unsigned long long blocks_in_lane = my_info->num_blocks
			/ my_info->file_lanes;
	/* calculate the starting offset */
	off_t start = my_info->thread_index * blocks_in_lane
			* my_info->block_size;

	for (int i = 0; i < my_info->num_ops; i++) {
		off_t lba = start
				+ (rand_r(&my_info->seed) % blocks_in_lane)
				* my_info->block_size;

		if (pread(my_info->file_desc, buf, my_info->block_size, lba)
				!= my_info->block_size) {
			fprintf(stderr, "!file read     lba %zu", lba);
		}
	}

	return NULL;
}
/*
 * worker -- write worker function for file io
 */
void *
wf_worker(void *arg)
{
	struct worker_info *my_info = arg;
	unsigned char buf[my_info->block_size];
	memset(buf, 1, my_info->block_size);

	unsigned long long blocks_in_lane = my_info->num_blocks
			/ my_info->file_lanes;
	/* calculate the starting offset */
	off_t start = my_info->thread_index * blocks_in_lane
			* my_info->block_size;

	for (int i = 0; i < my_info->num_ops; i++) {
		off_t lba = start
				+ (rand_r(&my_info->seed) % blocks_in_lane)
				* my_info->block_size;

		if (pwrite(my_info->file_desc, buf, my_info->block_size, lba)
				!= my_info->block_size) {
			fprintf(stderr, "!file write     lba %zu", lba);
		}
	}

	return NULL;
}

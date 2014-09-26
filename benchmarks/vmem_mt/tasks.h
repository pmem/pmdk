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

/*
 * tasks.h -- definitions for the thread tasks
 */

#include <stdlib.h>
#include <inttypes.h>
#include <libvmem.h>

#define	SUCCESS 0
#define	FAILURE -1

typedef enum allocator_e {
	ALLOCATOR_VMEM,
	ALLOCATOR_MALLOC,
	MAX_ALLOCATOR
} allocator_t;

typedef enum allocation_type_e {
	ALLOCATION_UNKNOWN,
	ALLOCATION_STATIC,
	ALLOCATION_RANGE
} allocation_type_t;

typedef struct arguments_s
{
	int thread_count;
	int pool_per_thread;
	uint64_t ops_count;
	unsigned int seed;
	unsigned int allocation_size;
	unsigned int allocation_size_max;
	allocation_type_t allocation_type;
	allocator_t allocator;
	char *dir_path;
} arguments_t;

typedef int (*task_f)(int, void *arg, struct random_data *rand_state);

enum {
	TASK_MALLOC,
	TASK_FREE,
	MAX_TASK
};

int task_malloc(int i, void *arg, struct random_data *rand_state);
int task_free(int i, void *arg, struct random_data *rand_state);

extern task_f tasks[MAX_TASK];

extern int allocation_range_min;
extern int allocation_range_max;
extern allocator_t allocator;

extern void **allocated_mem;

extern const int allocation_sizes[];

int run_threads(arguments_t *arguments, task_f task,
	int per_thread_arg, void **arg, double *elapsed);

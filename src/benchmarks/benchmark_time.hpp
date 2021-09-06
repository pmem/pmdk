/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2015-2020, Intel Corporation */
/*
 * benchmark_time.hpp -- declarations of benchmark_time module
 */
#include <ctime>

typedef struct timespec benchmark_time_t;

void benchmark_time_get(benchmark_time_t *time);
void benchmark_time_diff(benchmark_time_t *d, benchmark_time_t *t1,
			 benchmark_time_t *t2);
double benchmark_time_get_secs(benchmark_time_t *t);
unsigned long long benchmark_time_get_nsecs(benchmark_time_t *t);
int benchmark_time_compare(const benchmark_time_t *t1,
			   const benchmark_time_t *t2);
void benchmark_time_set(benchmark_time_t *time, unsigned long long nsecs);
unsigned long long benchmark_get_avg_get_time(void);

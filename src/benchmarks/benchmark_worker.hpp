// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2017, Intel Corporation */
/*
 * benchmark_worker.hpp -- benchmark_worker module declarations
 */

#include "benchmark.hpp"
#include "os_thread.h"
/*
 *
 * The following table shows valid state transitions upon specified
 * API calls and operations performed by the worker thread:
 *
 * +========================+==========================+=============+
 * |       Application      |           State          |    Worker   |
 * +========================+==========================+=============+
 * | benchmark_worker_alloc | WORKER_STATE_IDLE        | wait        |
 * +------------------------+--------------------------+-------------+
 * | benchmark_worker_init  | WORKER_STATE_INIT        | invoke init |
 * +------------------------+--------------------------+-------------+
 * | wait                   | WORKER_STATE_INITIALIZED | end of init |
 * +------------------------+--------------------------+-------------+
 * | benchmark_worker_run   | WORKER_STATE_RUN         | invoke func |
 * +------------------------+--------------------------+-------------+
 * | benchmark_worker_join  | WORKER_STATE_END         | end of func |
 * +------------------------+--------------------------+-------------+
 * | benchmark_worker_exit  | WORKER_STATE_EXIT        | invoke exit |
 * +------------------------+--------------------------+-------------+
 * | wait                   | WORKER_STATE_DONE        | end of exit |
 * +------------------------+--------------------------+-------------+
 */
enum benchmark_worker_state {
	WORKER_STATE_IDLE,
	WORKER_STATE_INIT,
	WORKER_STATE_INITIALIZED,
	WORKER_STATE_RUN,
	WORKER_STATE_END,
	WORKER_STATE_EXIT,
	WORKER_STATE_DONE,

	MAX_WORKER_STATE,
};

struct benchmark_worker {
	os_thread_t thread;
	struct benchmark *bench;
	struct benchmark_args *args;
	struct worker_info info;
	int ret;
	int ret_init;
	int (*func)(struct benchmark *bench, struct worker_info *info);
	int (*init)(struct benchmark *bench, struct benchmark_args *args,
		    struct worker_info *info);
	void (*exit)(struct benchmark *bench, struct benchmark_args *args,
		     struct worker_info *info);
	os_cond_t cond;
	os_mutex_t lock;
	enum benchmark_worker_state state;
};

struct benchmark_worker *benchmark_worker_alloc(void);
void benchmark_worker_free(struct benchmark_worker *);

int benchmark_worker_init(struct benchmark_worker *);
void benchmark_worker_exit(struct benchmark_worker *);
int benchmark_worker_run(struct benchmark_worker *);
int benchmark_worker_join(struct benchmark_worker *);

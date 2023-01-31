// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2023, Intel Corporation */

/*
 * pi.c -- example usage of user lists
 *
 * Calculates pi number with multiple threads using Leibniz formula.
 */
#include <ex_common.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <assert.h>
#include <inttypes.h>
#include <libpmemobj.h>
#include <pthread.h>

/*
 * Layout definition
 */
POBJ_LAYOUT_BEGIN(pi);
POBJ_LAYOUT_ROOT(pi, struct pi);
POBJ_LAYOUT_TOID(pi, struct pi_task);
POBJ_LAYOUT_END(pi);

static PMEMobjpool *pop;

struct pi_task_proto {
	uint64_t start;
	uint64_t stop;
	long double result;
};

struct pi_task {
	struct pi_task_proto proto;
	POBJ_LIST_ENTRY(struct pi_task) todo;
	POBJ_LIST_ENTRY(struct pi_task) done;
};

struct pi {
	POBJ_LIST_HEAD(todo, struct pi_task) todo;
	POBJ_LIST_HEAD(done, struct pi_task) done;
};

/*
 * pi_task_construct -- task constructor
 */
static int
pi_task_construct(PMEMobjpool *_pop, void *ptr, void *arg)
{
	struct pi_task *t = (struct pi_task *)ptr;
	struct pi_task_proto *p = (struct pi_task_proto *)arg;
	t->proto = *p;
	pmemobj_persist(_pop, t, sizeof(*t));

	return 0;
}

/*
 * calc_pi -- worker for pi calculation
 */
static void *
calc_pi(void *arg)

{
	TOID(struct pi) pi = POBJ_ROOT(pop, struct pi);
	TOID(struct pi_task) task = *((TOID(struct pi_task) *)arg);

	long double result = 0;
	for (uint64_t i = D_RO(task)->proto.start;
		i < D_RO(task)->proto.stop; ++i) {
		result += (pow(-1, (double)i) / (2 * i + 1));
	}
	D_RW(task)->proto.result = result;
	pmemobj_persist(pop, &D_RW(task)->proto.result, sizeof(result));

	POBJ_LIST_MOVE_ELEMENT_HEAD(pop, &D_RW(pi)->todo, &D_RW(pi)->done,
					task, todo, done);

	return NULL;
}

/*
 * calc_pi_mt -- calculate all the pending to-do tasks
 */
static void
calc_pi_mt(void)
{
	TOID(struct pi) pi = POBJ_ROOT(pop, struct pi);

	int pending = 0;
	TOID(struct pi_task) iter;
	POBJ_LIST_FOREACH(iter, &D_RO(pi)->todo, todo)
		pending++;

	if (pending == 0)
		return;

	int i = 0;
	TOID(struct pi_task) *tasks = (TOID(struct pi_task) *)malloc(
		sizeof(TOID(struct pi_task)) * pending);
	if (tasks == NULL) {
		fprintf(stderr, "failed to allocate tasks\n");
		return;
	}

	POBJ_LIST_FOREACH(iter, &D_RO(pi)->todo, todo)
		tasks[i++] = iter;

	pthread_t workers[pending];
	for (i = 0; i < pending; ++i)
		if (pthread_create(&workers[i], NULL, calc_pi, &tasks[i]) != 0)
			break;

	for (i = i - 1; i >= 0; --i)
		pthread_join(workers[i], NULL);

	free(tasks);
}

/*
 * prep_todo_list -- create tasks to be done
 */
static int
prep_todo_list(int threads, int ops)
{
	TOID(struct pi) pi = POBJ_ROOT(pop, struct pi);

	if (!POBJ_LIST_EMPTY(&D_RO(pi)->todo))
		return -1;

	int ops_per_thread = ops / threads;
	uint64_t last = 0; /* last calculated denominator */

	TOID(struct pi_task) iter;
	POBJ_LIST_FOREACH(iter, &D_RO(pi)->done, done) {
		if (last < D_RO(iter)->proto.stop)
			last = D_RO(iter)->proto.stop;
	}

	int i;
	for (i = 0; i < threads; ++i) {
		uint64_t start = last + (i * ops_per_thread);
		struct pi_task_proto proto;
		proto.start = start;
		proto.stop = start + ops_per_thread;
		proto.result = 0;

		POBJ_LIST_INSERT_NEW_HEAD(pop, &D_RW(pi)->todo, todo,
			sizeof(struct pi_task), pi_task_construct, &proto);
	}

	return 0;
}

int
main(int argc, char *argv[])
{
	if (argc < 3) {
		printf("usage: %s file-name "
			"[print|done|todo|finish|calc <# of threads> <ops>]\n",
			argv[0]);
		return 1;
	}

	const char *path = argv[1];

	pop = NULL;

	if (file_exists(path) != 0) {
		if ((pop = pmemobj_create(path, POBJ_LAYOUT_NAME(pi),
			PMEMOBJ_MIN_POOL, CREATE_MODE_RW)) == NULL) {
			printf("failed to create pool\n");
			return 1;
		}
	} else {
		if ((pop = pmemobj_open(path, POBJ_LAYOUT_NAME(pi))) == NULL) {
			printf("failed to open pool\n");
			return 1;
		}
	}

	TOID(struct pi) pi = POBJ_ROOT(pop, struct pi);

	char op = argv[2][0];
	switch (op) {
		case 'p': { /* print pi */
			long double pi_val = 0;
			TOID(struct pi_task) iter;
			POBJ_LIST_FOREACH(iter, &D_RO(pi)->done, done) {
				pi_val += D_RO(iter)->proto.result;
			}

			printf("pi: %.10Lf\n", pi_val * 4);
		} break;
		case 'd': { /* print done list */
			TOID(struct pi_task) iter;
			POBJ_LIST_FOREACH(iter, &D_RO(pi)->done, done) {
				printf("(%" PRIu64 " - %" PRIu64 ") = %.10Lf\n",
					D_RO(iter)->proto.start,
					D_RO(iter)->proto.stop,
					D_RO(iter)->proto.result);
			}
		} break;
		case 't': { /* print to-do list */
			TOID(struct pi_task) iter;
			POBJ_LIST_FOREACH(iter, &D_RO(pi)->todo, todo) {
				printf("(%" PRIu64 " - %" PRIu64 ") = %.10Lf\n",
					D_RO(iter)->proto.start,
					D_RO(iter)->proto.stop,
					D_RO(iter)->proto.result);
			}
		} break;
		case 'c': { /* calculate pi */
			if (argc < 5) {
				printf("usage: %s file-name "
					"calc <# of threads> <ops>\n",
					argv[0]);
				return 1;
			}
			int threads = atoi(argv[3]);
			int ops = atoi(argv[4]);
			assert((threads > 0) && (ops > 0));
			if (prep_todo_list(threads, ops) == -1)
				printf("pending todo tasks\n");
			else
				calc_pi_mt();
		} break;
		case 'f': { /* finish to-do tasks */
			calc_pi_mt();
		} break;
	}

	pmemobj_close(pop);

	return 0;
}

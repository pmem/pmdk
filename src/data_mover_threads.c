// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

/* disable conditional expression is const warning */
#ifdef _WIN32
#pragma warning(disable : 4127)
#endif

#include <stdlib.h>
#include <string.h>
#include "core/membuf.h"
#include "core/out.h"
#include "libminiasync/data_mover_threads.h"
#include "core/util.h"
#include "core/os_thread.h"
#include "core/ringbuf.h"

#define DATA_MOVER_THREADS_DEFAULT_NTHREADS 12
#define DATA_MOVER_THREADS_DEFAULT_RINGBUF_SIZE 128

struct data_mover_threads_op_fns {
	memcpy_fn op_memcpy;
};

struct data_mover_threads {
	struct vdm base; /* must be first */

	struct data_mover_threads_op_fns op_fns;
	struct ringbuf *buf;
	size_t nthreads;
	os_thread_t *threads;
	struct membuf *membuf;
	enum future_notifier_type desired_notifier;
};

struct data_mover_threads_data {
	enum future_notifier_type desired_notifier;
	struct future_notifier notifier;
	uint64_t complete;
	uint64_t started;

	struct vdm_operation op;
};

/*
 * Standard implementation of memcpy used if none was specified by the user.
 */
void *std_memcpy(void *dst, const void *src, size_t n, unsigned flags) {
	return memcpy(dst, src, n);
}

static struct data_mover_threads_op_fns op_fns_default = {
	.op_memcpy = std_memcpy
};

void data_mover_threads_set_memcpy_fn(struct data_mover_threads *dmt,
				memcpy_fn op_memcpy) {
	dmt->op_fns.op_memcpy = op_memcpy;
}

/*
 * data_mover_threads_do_operation -- implementation of the various
 * operations supported by this data mover
 */
static void
data_mover_threads_do_operation(struct data_mover_threads_data *data,
				struct data_mover_threads *dmt)
{
	switch (data->op.type) {
		case VDM_OPERATION_MEMCPY: {
			struct vdm_operation_data_memcpy *mdata
				= &data->op.data.memcpy;
			memcpy_fn op_memcpy = dmt->op_fns.op_memcpy;
			op_memcpy(mdata->dest,
				mdata->src, mdata->n, (unsigned)mdata->flags);
		} break;
		default:
			ASSERT(0); /* unreachable */
			break;
	}

	if (data->desired_notifier == FUTURE_NOTIFIER_WAKER) {
		FUTURE_WAKER_WAKE(&data->notifier.waker);
	}
	util_atomic_store_explicit64(&data->complete, 1, memory_order_release);
}

/*
 * data_mover_threads_loop -- loop that is executed by every worker
 * thread of the mover
 */
static void *
data_mover_threads_loop(void *arg)
{
	struct data_mover_threads *dmt_threads = arg;
	struct ringbuf *buf = dmt_threads->buf;
	struct data_mover_threads_data *tdata;

	while (1) {
		/*
		 * Worker thread is trying to dequeue from ringbuffer,
		 * if he fails, he's waiting until something is added to
		 * the ringbuffer.
		 */
		if ((tdata = ringbuf_dequeue(buf)) == NULL)
			return NULL;

		data_mover_threads_do_operation(tdata, dmt_threads);
	}
}

/*
 * data_mover_threads_operation_check -- check the status of a thread operation
 */
static enum future_state
data_mover_threads_operation_check(void *data,
	const struct vdm_operation *operation)
{
	SUPPRESS_UNUSED(operation);

	struct data_mover_threads_data *tdata = data;

	uint64_t complete;
	util_atomic_load_explicit64(&tdata->complete,
		&complete, memory_order_acquire);
	if (complete)
		return FUTURE_STATE_COMPLETE;

	uint64_t started;
	util_atomic_load_explicit64(&tdata->started,
		&started, memory_order_acquire);
	if (started)
		return FUTURE_STATE_RUNNING;

	return FUTURE_STATE_IDLE;
}

/*
 * data_mover_threads_operation_new -- create a new thread operation that uses
 * wakers
 */
static void *
data_mover_threads_operation_new(struct vdm *vdm,
	const enum vdm_operation_type type)
{
	SUPPRESS_UNUSED(type);

	struct data_mover_threads *dmt_threads =
		(struct data_mover_threads *)vdm;

	struct data_mover_threads_data *op =
		membuf_alloc(dmt_threads->membuf,
		sizeof(struct data_mover_threads_data));
	if (op == NULL)
		return NULL;

	op->complete = 0;
	op->started = 0;
	op->desired_notifier = dmt_threads->desired_notifier;

	return op;
}

/*
 * vdm_threads_operation_delete -- delete a thread operation
 */
static void
data_mover_threads_operation_delete(void *data,
	const struct vdm_operation *operation,
	struct vdm_operation_output *output)
{
	output->result = VDM_SUCCESS;
	switch (operation->type) {
		case VDM_OPERATION_MEMCPY:
			output->type = VDM_OPERATION_MEMCPY;
			output->output.memcpy.dest =
				operation->data.memcpy.dest;
			break;
		default:
			ASSERT(0);
	}

	membuf_free(data);
}

/*
 * data_mover_threads_operation_start -- start a memory operation using threads
 */
static int
data_mover_threads_operation_start(void *data,
	const struct vdm_operation *operation, struct future_notifier *n)
{
	struct data_mover_threads_data *tdata =
		(struct data_mover_threads_data *)data;
	memcpy(&tdata->op, operation, sizeof(*operation));

	if (n) {
		n->notifier_used = tdata->desired_notifier;
		tdata->notifier = *n;
		if (tdata->desired_notifier == FUTURE_NOTIFIER_POLLER) {
			n->poller.ptr_to_monitor = &tdata->complete;
		}
	} else {
		tdata->desired_notifier = FUTURE_NOTIFIER_NONE;
	}

	struct data_mover_threads *dmt_threads = membuf_ptr_user_data(tdata);

	if (ringbuf_tryenqueue(dmt_threads->buf, tdata) == 0) {
		util_atomic_store_explicit64(&tdata->started,
			FUTURE_STATE_RUNNING, memory_order_release);
	}

	return 0;
}

static struct vdm data_mover_threads_vdm = {
	.op_new = data_mover_threads_operation_new,
	.op_delete = data_mover_threads_operation_delete,
	.op_check = data_mover_threads_operation_check,
	.op_start = data_mover_threads_operation_start,
};

/*
 * data_mover_threads_new -- creates a new data mover instance that's uses
 * worker threads for memory operations
 */
struct data_mover_threads *
data_mover_threads_new(size_t nthreads, size_t ringbuf_size,
	enum future_notifier_type desired_notifier)
{
	struct data_mover_threads *dmt_threads =
		malloc(sizeof(struct data_mover_threads));
	if (dmt_threads == NULL)
		goto data_failed;

	dmt_threads->desired_notifier = desired_notifier;
	dmt_threads->base = data_mover_threads_vdm;
	dmt_threads->op_fns = op_fns_default;

	dmt_threads->buf = ringbuf_new((unsigned)ringbuf_size);
	if (dmt_threads->buf == NULL)
		goto ringbuf_failed;

	dmt_threads->membuf = membuf_new(dmt_threads);
	if (dmt_threads->membuf == NULL)
		goto membuf_failed;

	dmt_threads->nthreads = nthreads;
	dmt_threads->threads = malloc(sizeof(os_thread_t) *
		dmt_threads->nthreads);
	if (dmt_threads->threads == NULL)
		goto threads_array_failed;

	size_t i;
	for (i = 0; i < dmt_threads->nthreads; i++) {
		os_thread_create(&dmt_threads->threads[i],
			NULL, data_mover_threads_loop, dmt_threads);
	}

	return dmt_threads;

threads_array_failed:
	membuf_delete(dmt_threads->membuf);

membuf_failed:
	ringbuf_delete(dmt_threads->buf);

ringbuf_failed:
	free(dmt_threads);

data_failed:
	return NULL;
}

/*
 * data_mover_threads_default -- creates a new data mover instance with
 * default parameters
 */
struct data_mover_threads *
data_mover_threads_default()
{
	return data_mover_threads_new(DATA_MOVER_THREADS_DEFAULT_NTHREADS,
		DATA_MOVER_THREADS_DEFAULT_RINGBUF_SIZE,
		FUTURE_NOTIFIER_WAKER);
}

/*
 * data_mover_threads_get_vdm -- returns the vdm operations for the mover
 */
struct vdm *
data_mover_threads_get_vdm(struct data_mover_threads *dmt)
{
	return &dmt->base;
}

/*
 * data_mover_threads_delete -- perform necessary cleanup after threads mover.
 * Releases all memory and closes all created threads.
 */
void
data_mover_threads_delete(struct data_mover_threads *dmt)
{
	ringbuf_stop(dmt->buf);
	for (size_t i = 0; i < dmt->nthreads; i++) {
		os_thread_join(&dmt->threads[i], NULL);
	}
	free(dmt->threads);
	membuf_delete(dmt->membuf);
	ringbuf_delete(dmt->buf);
	free(dmt);
}

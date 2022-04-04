// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#include "test_helpers.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libminiasync.h>

struct alloc_data {
	size_t n;
};

struct alloc_output {
	void *ptr;
};

FUTURE(alloc_fut, struct alloc_data, struct alloc_output);

enum future_state
alloc_impl(struct future_context *context, struct future_notifier *notifier)
{
	struct alloc_data *data = future_context_get_data(context);
	struct alloc_output *output = future_context_get_output(context);

	output->ptr = malloc(data->n);
	UT_ASSERTne(output->ptr, NULL);

	return FUTURE_STATE_COMPLETE;
}

struct alloc_fut
async_alloc(size_t size)
{
	struct alloc_fut fut = {.output.ptr = NULL};
	fut.data.n = size;
	FUTURE_INIT(&fut, alloc_impl);
	return fut;
};

struct strdup_data {
	FUTURE_CHAIN_ENTRY(struct alloc_fut, alloc);
	FUTURE_CHAIN_ENTRY_LAST(struct vdm_operation_future, copy);
	void *src;
	size_t length;
};

struct strdup_output {
	void *ptr;
	size_t length;
};

FUTURE(strdup_fut, struct strdup_data, struct strdup_output);

void
strdup_map_alloc_to_copy(struct future_context *lhs,
	struct future_context *rhs, void *arg)
{
	struct alloc_output *alloc = future_context_get_output(lhs);
	struct vdm_operation_data *copy = future_context_get_data(rhs);
	copy->operation.data.memcpy.dest = alloc->ptr;
}

void
strdup_map_copy_to_output(struct future_context *lhs,
	struct future_context *rhs, void *arg)
{
	struct vdm_operation_data *copy = future_context_get_data(lhs);
	struct vdm_operation_output *copy_output =
		future_context_get_output(lhs);
	UT_ASSERTeq(copy_output->result, VDM_SUCCESS);
	struct strdup_output *strdup = future_context_get_output(rhs);
	strdup->ptr = copy->operation.data.memcpy.dest;
	strdup->length = copy->operation.data.memcpy.n;
}

struct strdup_fut
async_strdup(struct vdm *vdm, char *s)
{
	struct strdup_fut fut = {.output = {.length = 0, .ptr = NULL}};

	size_t len = strlen(s) + 1;
	FUTURE_CHAIN_ENTRY_INIT(&fut.data.alloc, async_alloc(len),
		strdup_map_alloc_to_copy, NULL);
	FUTURE_CHAIN_ENTRY_INIT(&fut.data.copy,
		vdm_memcpy(vdm, NULL, s, len, 0),
		strdup_map_copy_to_output, NULL);
	FUTURE_CHAIN_INIT(&fut);

	return fut;
}

void
strdup_init(void *future, struct future_context *chain_fut, void *arg)
{
	struct vdm *vdm = arg;
	struct strdup_data *strdup_data =
		future_context_get_data(chain_fut);

	struct vdm_operation_future fut = vdm_memcpy(vdm,
		strdup_data->alloc.fut.output.ptr,
		strdup_data->src, strdup_data->length, 0);
	memcpy(future, &fut, sizeof(fut));
}

struct strdup_fut
async_lazy_strdup(struct vdm *vdm, char *s)
{
	struct strdup_fut fut = {.output = {.length = 0, .ptr = NULL}};
	fut.data.src = s;
	fut.data.length = strlen(s) + 1;

	FUTURE_CHAIN_ENTRY_INIT(&fut.data.alloc, async_alloc(fut.data.length),
		strdup_map_alloc_to_copy, NULL);
	FUTURE_CHAIN_ENTRY_LAZY_INIT(&fut.data.copy,
		strdup_init, vdm,
		strdup_map_copy_to_output, NULL);
	FUTURE_CHAIN_INIT(&fut);

	return fut;
}

static char *hello_world = "Hello World!";

void
test_strdup_fut(struct strdup_fut fut)
{
	FUTURE_BUSY_POLL(&fut);

	struct strdup_output *output = FUTURE_OUTPUT(&fut);
	UT_ASSERTeq(strcmp(hello_world, output->ptr), 0);
	UT_ASSERTeq(strlen(hello_world) + 1, output->length);

	free(output->ptr);
}

void
test_too_many_ops(struct vdm *vdm)
{
	struct vdm_operation_future first_op;
	int assigned = 0;

	enum future_state state;
	char dest = 0;
	char src = 1;
	do {
		struct vdm_operation_future f =
			vdm_memcpy(vdm, &dest, &src, 1, 0);
		if (!assigned) {
			assigned = 1;
			first_op = f;
		}
		state = FUTURE_STATE(&f);
		if (state == FUTURE_STATE_COMPLETE) {
			struct vdm_operation_output *output = FUTURE_OUTPUT(&f);
			UT_ASSERTeq(output->result, VDM_ERROR_OUT_OF_MEMORY);
		}
	} while (state != FUTURE_STATE_COMPLETE);
	FUTURE_BUSY_POLL(&first_op);
	UT_ASSERTeq(dest, src);

	struct vdm_operation_future f = vdm_memcpy(vdm, &dest, &src, 1, 0);
	UT_ASSERTeq(FUTURE_STATE(&f), FUTURE_STATE_IDLE);
	FUTURE_BUSY_POLL(&f);
	UT_ASSERTeq(FUTURE_OUTPUT(&f)->result, VDM_SUCCESS);
}

int
main(void)
{
	UT_ASSERTeq(sizeof(struct vdm_operation_data), 64);

	struct data_mover_sync *sync = data_mover_sync_new();
	struct vdm *vdm = data_mover_sync_get_vdm(sync);

	test_strdup_fut(async_strdup(vdm, hello_world));
	test_strdup_fut(async_lazy_strdup(vdm, hello_world));
	test_too_many_ops(vdm);

	data_mover_sync_delete(sync);

	return 0;
}

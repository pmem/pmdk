// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

#include <stdlib.h>
#include <string.h>
#include "core/membuf.h"
#include "core/out.h"
#include "libminiasync/vdm.h"

/*
 * vdm_operation_impl -- the poll implementation for a generic vdm operation
 * The operation lifecycle is as follows:
 *	FUTURE_STATE_IDLE -- op_start() --> FUTURE_STATE_RUNNING
 *	FUTURE_STATE_RUNNING -- op_check() --> FUTURE_STATE_COMPLETE
 *	FUTURE_STATE_COMPLETE --> op_delete()
 */
static enum future_state
vdm_operation_impl(struct future_context *context, struct future_notifier *n)
{
	struct vdm_operation_data *data = future_context_get_data(context);
	struct vdm *vdm = membuf_ptr_user_data(data->op);

	if (context->state == FUTURE_STATE_IDLE) {
		if (vdm->op_start(data->op, n) != 0) {
			return FUTURE_STATE_IDLE;
		}
	}

	enum future_state state = vdm->op_check(data->op);

	if (state == FUTURE_STATE_COMPLETE) {
		struct vdm_operation_output *output =
			future_context_get_output(context);
		vdm->op_delete(data->op, output);
		/* variable data is no longer valid! */
	}

	return state;
}

/*
 * vdm_memcpy -- instantiates a new memcpy vdm operation and returns a new
 * future to represent that operation
 */
struct vdm_operation_future
vdm_memcpy(struct vdm *vdm, void *dest, void *src, size_t n, uint64_t flags)
{
	struct vdm_operation op;
	op.type = VDM_OPERATION_MEMCPY;
	op.memcpy.dest = dest;
	op.memcpy.flags = flags;
	op.memcpy.n = n;
	op.memcpy.src = src;

	struct vdm_operation_future future = {0};
	future.data.op = vdm->op_new(vdm, &op);
	FUTURE_INIT(&future, vdm_operation_impl);

	return future;
}

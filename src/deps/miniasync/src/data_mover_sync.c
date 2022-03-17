// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

/* disable conditional expression is const warning */
#ifdef _WIN32
#pragma warning(disable : 4127)
#endif

#include "libminiasync/vdm.h"
#include "core/membuf.h"
#include "core/out.h"

struct data_mover_sync {
	struct vdm base; /* must be first */

	struct membuf *membuf;
};

struct data_mover_sync_op {
	struct vdm_operation op;
	int complete;
};

/*
 * sync_operation_check -- always returns COMPLETE because sync mover operations
 * are complete immediately after starting.
 */
static enum future_state
sync_operation_check(void *op)
{
	struct data_mover_sync_op *sync_op = op;

	int complete;
	util_atomic_load_explicit32(&sync_op->complete, &complete,
		memory_order_acquire);

	return complete ? FUTURE_STATE_COMPLETE : FUTURE_STATE_IDLE;
}

/*
 * sync_operation_new -- creates a new sync operation
 */
static void *
sync_operation_new(struct vdm *vdm, const struct vdm_operation *operation)
{
	struct data_mover_sync *vdm_sync = (struct data_mover_sync *)vdm;
	struct data_mover_sync_op *sync_op = membuf_alloc(vdm_sync->membuf,
		sizeof(struct data_mover_sync_op));
	if (sync_op == NULL)
		return NULL;

	sync_op->op = *operation;
	sync_op->complete = 0;

	return sync_op;
}

/*
 * sync_operation_delete -- deletes sync operation
 */
static void
sync_operation_delete(void *op, struct vdm_operation_output *output)
{
	struct data_mover_sync_op *sync_op = (struct data_mover_sync_op *)op;
	switch (sync_op->op.type) {
		case VDM_OPERATION_MEMCPY:
			output->type = VDM_OPERATION_MEMCPY;
			output->output.memcpy.dest =
				sync_op->op.data.memcpy.dest;
			break;
		default:
			ASSERT(0);
	}

	membuf_free(op);
}

/*
 * sync_operation_start -- start (and perform) a synchronous memory operation
 */
static int
sync_operation_start(void *op, struct future_notifier *n)
{
	struct data_mover_sync_op *sync_op = (struct data_mover_sync_op *)op;
	if (n)
		n->notifier_used = FUTURE_NOTIFIER_NONE;
	memcpy(sync_op->op.data.memcpy.dest,
		sync_op->op.data.memcpy.src,
		sync_op->op.data.memcpy.n);

	util_atomic_store_explicit32(&sync_op->complete,
		1, memory_order_release);

	return 0;
}

static struct vdm data_mover_sync_vdm = {
	.op_new = sync_operation_new,
	.op_delete = sync_operation_delete,
	.op_check = sync_operation_check,
	.op_start = sync_operation_start,
};

/*
 * data_mover_sync_new -- creates a new synchronous data mover
 */
struct data_mover_sync *
data_mover_sync_new(void)
{
	struct data_mover_sync *dms = malloc(sizeof(struct data_mover_sync));
	if (dms == NULL)
		return NULL;

	dms->base = data_mover_sync_vdm;
	dms->membuf = membuf_new(dms);
	if (dms->membuf == NULL)
		goto membuf_failed;

	return dms;

membuf_failed:
	free(dms);
	return NULL;
}

/*
 * data_mover_sync_get_vdm -- returns the vdm operations for the sync mover
 */
struct vdm *
data_mover_sync_get_vdm(struct data_mover_sync *dms)
{
	return &dms->base;
}

/*
 * data_mover_sync_delete -- deletes a synchronous data mover
 */
void
data_mover_sync_delete(struct data_mover_sync *dms)
{
	membuf_delete(dms->membuf);
	free(dms);
}

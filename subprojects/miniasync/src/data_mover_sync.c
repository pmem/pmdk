// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

/* disable conditional expression is const warning */
#include "core/util.h"
#ifdef _WIN32
#pragma warning(disable : 4127)
#endif

#include "libminiasync/vdm.h"
#include "core/membuf.h"
#include "core/out.h"

#define SUPPORTED_FLAGS 0

struct data_mover_sync {
	struct vdm base; /* must be first */

	struct membuf *membuf;
};

struct data_mover_sync_data {
	int complete;
};

/*
 * sync_operation_check -- always returns COMPLETE because sync mover operations
 * are complete immediately after starting.
 */
static enum future_state
sync_operation_check(void *data, const struct vdm_operation *operation)
{
	SUPPRESS_UNUSED(operation);

	struct data_mover_sync_data *sync_data = data;

	int complete;
	util_atomic_load_explicit32(&sync_data->complete, &complete,
		memory_order_acquire);

	return complete ? FUTURE_STATE_COMPLETE : FUTURE_STATE_IDLE;
}

/*
 * sync_operation_new -- creates a new sync operation
 */
static void *
sync_operation_new(struct vdm *vdm, const enum vdm_operation_type type)
{
	SUPPRESS_UNUSED(type);

	struct data_mover_sync *vdm_sync = (struct data_mover_sync *)vdm;
	struct data_mover_sync_data *sync_data = membuf_alloc(vdm_sync->membuf,
		sizeof(struct data_mover_sync_data));
	if (sync_data == NULL)
		return NULL;

	sync_data->complete = 0;

	return sync_data;
}

/*
 * sync_operation_delete -- deletes sync operation
 */
static void
sync_operation_delete(void *data, const struct vdm_operation *operation,
	struct vdm_operation_output *output)
{
	output->result = VDM_SUCCESS;

	switch (operation->type) {
		case VDM_OPERATION_MEMCPY:
			output->type = VDM_OPERATION_MEMCPY;
			output->output.memcpy.dest =
				operation->data.memcpy.dest;
			break;
		case VDM_OPERATION_MEMMOVE:
			output->type = VDM_OPERATION_MEMMOVE;
			output->output.memmove.dest =
				operation->data.memcpy.dest;
			break;
		case VDM_OPERATION_MEMSET:
			output->type = VDM_OPERATION_MEMSET;
			output->output.memset.str =
				operation->data.memset.str;
			break;
		default:
			ASSERT(0);
	}

	membuf_free(data);
}

/*
 * sync_operation_start -- start (and perform) a synchronous memory operation
 */
static int
sync_operation_start(void *data, const struct vdm_operation *operation,
	struct future_notifier *n)
{
	struct data_mover_sync_data *sync_data =
		(struct data_mover_sync_data *)data;

	if (n)
		n->notifier_used = FUTURE_NOTIFIER_NONE;

	switch (operation->type) {
		case VDM_OPERATION_MEMCPY:
			memcpy(operation->data.memcpy.dest,
				operation->data.memcpy.src,
				operation->data.memcpy.n);
			break;
		case VDM_OPERATION_MEMMOVE:
			memmove(operation->data.memcpy.dest,
				operation->data.memcpy.src,
				operation->data.memcpy.n);
			break;
		case VDM_OPERATION_MEMSET:
			memset(operation->data.memset.str,
				operation->data.memset.c,
				operation->data.memset.n);
			break;
		default:
			ASSERT(0);
	}

	util_atomic_store_explicit32(&sync_data->complete,
		1, memory_order_release);

	return 0;
}

static struct vdm data_mover_sync_vdm = {
	.op_new = sync_operation_new,
	.op_delete = sync_operation_delete,
	.op_check = sync_operation_check,
	.op_start = sync_operation_start,
	.capabilities = SUPPORTED_FLAGS,
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

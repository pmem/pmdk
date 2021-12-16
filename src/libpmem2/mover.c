// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021, Intel Corporation */

/*
 * mover.c -- default pmem2 data mover
 */

#include "libpmem2.h"
#include "mover.h"
#include "map.h"
#include "out.h"
#include "pmem2_utils.h"
#include "util.h"
#include <stdlib.h>
#include <unistd.h>

struct data_mover {
	struct vdm base; /* must be first */
	struct pmem2_map *map;
};

struct data_mover_op {
	struct vdm_operation op;
	struct data_mover *mover; /* xxx: shoud be keept in vdm_operation ? */
	int complete;
};

/*
 * sync_operation_check -- always returns COMPLETE because sync mover operations
 * are complete immediately after starting.
 */
static enum future_state
sync_operation_check(void *op)
{
	struct data_mover_op *sync_op = op;

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
	struct data_mover *vdm_sync = (struct data_mover *)vdm;
	/* XXX: backport membuf from miniasync */
	struct data_mover_op *sync_op = malloc(sizeof(*sync_op));
	if (sync_op == NULL)
		return NULL;

	sync_op->complete = 0;
	sync_op->mover = vdm_sync;
	sync_op->op = *operation;

	return sync_op;
}

/*
 * sync_operation_delete -- deletes sync operation
 */
static void
sync_operation_delete(void *op, struct vdm_operation_output *output)
{
	struct data_mover_op *sync_op = (struct data_mover_op *)op;
	switch (sync_op->op.type) {
		case VDM_OPERATION_MEMCPY:
			output->type = VDM_OPERATION_MEMCPY;
			output->memcpy.dest = sync_op->op.memcpy.dest;
			break;
		default:
			FATAL("unsupported operation type");
	}
	free(sync_op);
}

/*
 * sync_operation_start -- start (and perform) a synchronous memory operation
 */
static int
sync_operation_start(void *op, struct future_notifier *n)
{
	struct data_mover_op *sync_op = (struct data_mover_op *)op;
	if (n)
		n->notifier_used = FUTURE_NOTIFIER_NONE;

	pmem2_memcpy_fn memcpy_fn = pmem2_get_memcpy_fn(sync_op->mover->map);

	memcpy_fn(sync_op->op.memcpy.dest, sync_op->op.memcpy.src,
		sync_op->op.memcpy.n, PMEM2_F_MEM_NONTEMPORAL);

	util_atomic_store_explicit32(&sync_op->complete,
		1, memory_order_release);

	return 0;
}

static struct vdm data_mover_vdm = {
	.op_new = sync_operation_new,
	.op_delete = sync_operation_delete,
	.op_check = sync_operation_check,
	.op_start = sync_operation_start,
};

/*
 * mover_new -- creates a new synchronous data mover
 */
int
mover_new(struct pmem2_map *map, struct vdm **vdm)
{
	int ret;
	struct data_mover *dms = pmem2_malloc(sizeof(*map), &ret);
	if (dms == NULL)
		return ret;

	dms->base = data_mover_vdm;
	dms->map = map;
	*vdm = (struct vdm *)dms;
	return 0;
}

/*
 * mover_delete -- deletes a synchronous data mover
 */
void
mover_delete(struct vdm *dms)
{
	free((struct data_mover *)dms);
}

struct vdm_operation_future pmem2_memcpy_async(struct pmem2_map *map,
	void *pmemdest,	const void *src, size_t len, unsigned flags)
{
	SUPPRESS_UNUSED(flags);
	return vdm_memcpy(map->vdm, pmemdest, (void *)src, len, 0);
}

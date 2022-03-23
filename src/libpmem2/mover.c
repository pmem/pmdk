// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

/*
 * mover.c -- default pmem2 data mover
 */

#include "libpmem2.h"
#include "mover.h"
#include "map.h"
#include "membuf.h"
#include "out.h"
#include "pmem2_utils.h"
#include "util.h"
#include <inttypes.h>
#include <stdlib.h>
#include <unistd.h>

struct data_mover {
	struct vdm base; /* must be first */
	struct pmem2_map *map;
	struct membuf *membuf;
};

struct data_mover_op {
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
	LOG(3, "op %p", op);
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
	LOG(3, "vdm %p, operation %p", vdm, operation);
	struct data_mover *vdm_sync = (struct data_mover *)vdm;
	/* XXX: backport membuf from miniasync */
	struct data_mover_op *sync_op = membuf_alloc(vdm_sync->membuf,
		sizeof(struct data_mover_op));
	if (sync_op == NULL)
		return NULL;

	sync_op->complete = 0;
	sync_op->op = *operation;

	return sync_op;
}

/*
 * sync_operation_delete -- deletes sync operation
 */
static void
sync_operation_delete(void *op, struct vdm_operation_output *output)
{
	LOG(3, "op %p, output %p", op, output);
	struct data_mover_op *sync_op = (struct data_mover_op *)op;
	switch (sync_op->op.type) {
		case VDM_OPERATION_MEMCPY:
			output->type = VDM_OPERATION_MEMCPY;
			output->output.memcpy.dest =
				sync_op->op.data.memcpy.dest;
			break;
		default:
			FATAL("unsupported operation type");
	}
	membuf_free(op);
}

/*
 * sync_operation_start -- start (and perform) a synchronous memory operation
 */
static int
sync_operation_start(void *op, struct future_notifier *n)
{
	LOG(3, "op %p, notifier %p", op, n);
	struct data_mover_op *sync_op = (struct data_mover_op *)op;
	struct data_mover *mover = membuf_ptr_user_data(op);
	if (n)
		n->notifier_used = FUTURE_NOTIFIER_NONE;

	switch (sync_op->op.type) {
		case VDM_OPERATION_MEMCPY:
		{
			pmem2_memcpy_fn memcpy_fn;
			memcpy_fn = pmem2_get_memcpy_fn(mover->map);

			memcpy_fn(sync_op->op.data.memcpy.dest,
				sync_op->op.data.memcpy.src,
				sync_op->op.data.memcpy.n,
				PMEM2_F_MEM_NONTEMPORAL);
			break;
		}
		default:
			FATAL("unsupported operation type");
	}
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
	LOG(3, "map %p, vdm %p", map, vdm);
	int ret;
	struct data_mover *dms = pmem2_malloc(sizeof(*dms), &ret);
	if (dms == NULL)
		return ret;

	dms->base = data_mover_vdm;
	dms->map = map;
	*vdm = (struct vdm *)dms;

	dms->membuf = membuf_new(dms);
	if (dms->membuf == NULL) {
		ret = PMEM2_E_ERRNO;
		goto membuf_failed;
	}

	return 0;

membuf_failed:
	free(dms);
	return ret;
}

/*
 * mover_delete -- deletes a synchronous data mover
 */
void
mover_delete(struct vdm *dms)
{
	membuf_delete(((struct data_mover *)dms)->membuf);
	free((struct data_mover *)dms);
}

struct vdm_operation_future
pmem2_memcpy_async(struct pmem2_map *map,
	void *pmemdest,	const void *src, size_t len, unsigned flags)
{
	LOG(3, "map %p, pmemdest %p, src %p, len %" PRIu64 ", flags %u",
		map, pmemdest, src, len, flags);
	SUPPRESS_UNUSED(flags);
	return vdm_memcpy(map->vdm, pmemdest, (void *)src, len, 0);
}

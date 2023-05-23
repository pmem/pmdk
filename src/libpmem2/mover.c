// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2023, Intel Corporation */

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
#include "alloc.h"
#include <inttypes.h>
#include <stdlib.h>
#include <unistd.h>

#define SUPPORTED_FLAGS VDM_F_MEM_DURABLE

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
 * sync_operation_check -- always returns COMPLETE because sync mover
 * operations are complete immediately after starting.
 */
static enum future_state
sync_operation_check(void *data, const struct vdm_operation *operation)
{
	LOG(3, "data %p", data);
	SUPPRESS_UNUSED(operation);

	struct data_mover_op *sync_op = data;

	int complete;
	util_atomic_load_explicit32(&sync_op->complete, &complete,
		memory_order_acquire);

	return complete ? FUTURE_STATE_COMPLETE : FUTURE_STATE_IDLE;
}

/*
 * sync_operation_new -- creates a new sync operation
 */
static void *
sync_operation_new(struct vdm *vdm, const enum vdm_operation_type type)
{
	LOG(3, "vdm %p", vdm);

	SUPPRESS_UNUSED(type);

	struct data_mover *vdm_sync = (struct data_mover *)vdm;
	struct data_mover_op *sync_op =
		membuf_alloc(vdm_sync->membuf, sizeof(struct data_mover_op));

	if (sync_op == NULL)
		return NULL;

	sync_op->complete = 0;

	return sync_op;
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
			output->output.memset.str = operation->data.memset.str;
			break;
		default:
			FATAL("unsupported operation type");
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
	LOG(3, "data %p op %p, notifier %p", data, operation, n);
	struct data_mover_op *sync_data = (struct data_mover_op *)data;
	struct data_mover *mover = membuf_ptr_user_data(data);
	if (n)
		n->notifier_used = FUTURE_NOTIFIER_NONE;

	unsigned flags;

	switch (operation->type) {
		case VDM_OPERATION_MEMCPY: {
			pmem2_memcpy_fn memcpy_fn;
			memcpy_fn = pmem2_get_memcpy_fn(mover->map);
			flags = operation->data.memcpy.flags &
				VDM_F_MEM_DURABLE ?
				PMEM2_F_MEM_NONTEMPORAL : PMEM2_F_MEM_NOFLUSH;

			memcpy_fn(operation->data.memcpy.dest,
				operation->data.memcpy.src,
				operation->data.memcpy.n,
				flags);
			break;
		}
		case VDM_OPERATION_MEMMOVE: {
			pmem2_memmove_fn memmove_fn;
			memmove_fn = pmem2_get_memmove_fn(mover->map);
			flags = operation->data.memmove.flags &
				VDM_F_MEM_DURABLE ?
				PMEM2_F_MEM_NONTEMPORAL : PMEM2_F_MEM_NOFLUSH;

			memmove_fn(operation->data.memcpy.dest,
				operation->data.memcpy.src,
				operation->data.memcpy.n,
				flags);
			break;
		}
		case VDM_OPERATION_MEMSET: {
			pmem2_memset_fn memset_fn;
			memset_fn = pmem2_get_memset_fn(mover->map);
			flags = operation->data.memset.flags &
				VDM_F_MEM_DURABLE ?
				PMEM2_F_MEM_NONTEMPORAL : PMEM2_F_MEM_NOFLUSH;

			memset_fn(operation->data.memset.str,
				operation->data.memset.c,
				operation->data.memset.n,
				flags);
			break;
		}
		default:
			FATAL("unsupported operation type");
	}
	util_atomic_store_explicit32(&sync_data->complete, 1,
		memory_order_release);

	return 0;
}

static struct vdm data_mover_vdm = {
	.op_new = sync_operation_new,
	.op_delete = sync_operation_delete,
	.op_check = sync_operation_check,
	.op_start = sync_operation_start,
	.capabilities = SUPPORTED_FLAGS,
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

	LOG(3, "dms %p", dms);
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
	LOG(3, "dms %p", dms);
	membuf_delete(((struct data_mover *)dms)->membuf);
	Free((struct data_mover *)dms);
}

/*
 * pmem2_future_detect_properties -- identifies how the future should behave
 * depending on the properties of the underlying memory map and supported vdm
 * features.
 */
static void
pmem2_future_detect_properties(struct pmem2_map *map,
	uint64_t *vdm_flags, bool *needs_flushing)
{
	enum pmem2_granularity gran = pmem2_map_get_store_granularity(map);
	bool durable = vdm_is_supported(map->vdm, VDM_F_MEM_DURABLE);

	switch (gran) {
		case PMEM2_GRANULARITY_BYTE:
			*needs_flushing = 0;
			*vdm_flags = 0;
			break;
		case PMEM2_GRANULARITY_PAGE:
			*needs_flushing = 1;
			*vdm_flags = 0;
			break;
		case PMEM2_GRANULARITY_CACHE_LINE:
			*needs_flushing = !durable;
			*vdm_flags = durable ? VDM_F_MEM_DURABLE : 0;
			break;
		default:
			ASSERT(0); /* unreachable */
	};
}

/*
 * Attach pmem2_future_persist into pmem2_future if required by
 * characteristics of mapping and vdm
 */
static void
pmem2_future_prepare_finalizer(struct pmem2_map *map,
	struct pmem2_future *future,
	void *pmemdest, size_t len, bool needs_flushing) {
	struct future_chain_entry *operation_entry =
		(struct future_chain_entry *)(&future->data.op);

	if (needs_flushing) {
		/*
		 * The engine does not support PMEM, we do not have eADR, or we
		 * are writing to a page-cached backed device. In such case we
		 * have to ensure that the copied data is moved into
		 * a persistent domain properly before
		 * the pmem2_future is complete
		 */
		FUTURE_CHAIN_ENTRY_INIT(&future->data.fin,
			pmem2_persist_future(map, pmemdest, len),
			NULL, NULL);
	} else {
		/*
		 * The engine supports PMEM, or we have eADR, so persistence
		 * of the data is handled by these features.
		 */
		operation_entry->flags |=
			FUTURE_CHAIN_FLAG_ENTRY_LAST;
	}
}

/*
 * pmem2_memcpy_async -- returns a memcpy future
 */
struct pmem2_future
pmem2_memcpy_async(struct pmem2_map *map, void *pmemdest, const void *src,
	size_t len, unsigned flags)
{
	LOG(3, "map %p, pmemdest %p, src %p, len %" PRIu64 ", flags %u", map,
		pmemdest, src, len, flags);
	SUPPRESS_UNUSED(flags);

	uint64_t vdm_flags = 0;
	bool needs_flushing = 0;
	pmem2_future_detect_properties(map, &vdm_flags, &needs_flushing);

	struct pmem2_future future;
	FUTURE_CHAIN_ENTRY_INIT(&future.data.op,
		vdm_memcpy(map->vdm, pmemdest, (void *)src, len, vdm_flags),
		NULL, NULL);

	pmem2_future_prepare_finalizer(map, &future, pmemdest, len,
		needs_flushing);

	future.output.dest = pmemdest;

	FUTURE_CHAIN_INIT(&future);
	return future;
}

/*
 * pmem2_memove_async -- returns a memmove future
 */
struct pmem2_future
pmem2_memmove_async(struct pmem2_map *map, void *pmemdest, const void *src,
	size_t len, unsigned flags)
{
	LOG(3, "map %p, pmemdest %p, src %p, len %" PRIu64 ", flags %u", map,
		pmemdest, src, len, flags);
	SUPPRESS_UNUSED(flags);

	uint64_t vdm_flags = 0;
	bool needs_flushing = 0;
	pmem2_future_detect_properties(map, &vdm_flags, &needs_flushing);

	struct pmem2_future future;
	FUTURE_CHAIN_ENTRY_INIT(&future.data.op,
		vdm_memmove(map->vdm, pmemdest, (void *)src, len, vdm_flags),
		NULL, NULL);

	pmem2_future_prepare_finalizer(map, &future, pmemdest, len,
		needs_flushing);

	future.output.dest = pmemdest;

	FUTURE_CHAIN_INIT(&future);
	return future;
}

/*
 * pmem2_memset_async -- returns a memset future
 */
struct pmem2_future
pmem2_memset_async(struct pmem2_map *map, void *pmemstr, int c, size_t n,
	unsigned flags)
{
	LOG(3, "map %p, pmemstr %p, c %d, len %" PRIu64 ", flags %u", map,
		pmemstr, c, n, flags);
	SUPPRESS_UNUSED(flags);

	uint64_t vdm_flags = 0;
	bool needs_flushing = 0;
	pmem2_future_detect_properties(map, &vdm_flags, &needs_flushing);

	struct pmem2_future future;
	FUTURE_CHAIN_ENTRY_INIT(&future.data.op,
		vdm_memset(map->vdm, pmemstr, c, n, vdm_flags),
		NULL, NULL);

	pmem2_future_prepare_finalizer(map, &future, pmemstr, n,
		needs_flushing);

	future.output.dest = pmemstr;

	FUTURE_CHAIN_INIT(&future);
	return future;
}

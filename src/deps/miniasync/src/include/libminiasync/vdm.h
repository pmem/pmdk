/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2021-2022, Intel Corporation */

/*
 * vdm.h - public definitions for an abstract virtual data mover (VDM) type.
 *
 * Virtual mover is an abstract type that software can use to generically
 * perform asynchronous memory operations. Libraries can use this to avoid
 * a hard dependency on any specific implementation of hardware offload for
 * memory operations.
 *
 * Data movers implementations can use DMA engines like
 * Intel DSA (Data Streaming Accelerator), plain threads,
 * or synchronous operations in the current working thread.
 *
 * Data movers need to implement the descriptor interface, and applications can
 * use such implementations to create a concrete mover. Software can then use
 * movers to create more complex generic concurrent futures that use
 * asynchronous memory operations.
 */

#ifndef VDM_H
#define VDM_H 1

#include "future.h"

#ifdef __cplusplus
extern "C" {
#endif

struct vdm;

enum vdm_operation_type {
	VDM_OPERATION_MEMCPY,
};

struct vdm_operation_data_memcpy {
	void *dest;
	void *src;
	size_t n;
	uint64_t flags;
};

struct vdm_operation {
	enum vdm_operation_type type;
	union {
		struct vdm_operation_data_memcpy memcpy;
	} data;
};

struct vdm_operation_data {
	void *op;
	struct vdm *vdm;
};

struct vdm_operation_output_memcpy {
	void *dest;
};

struct vdm_operation_output {
	enum vdm_operation_type type; /* XXX: determine if needed */
	union {
		struct vdm_operation_output_memcpy memcpy;
	} output;
};

FUTURE(vdm_operation_future,
	struct vdm_operation_data, struct vdm_operation_output);

typedef void *(*vdm_operation_new)
	(struct vdm *vdm, const struct vdm_operation *operation);
typedef int (*vdm_operation_start)(void *op, struct future_notifier *n);
typedef enum future_state (*vdm_operation_check)(void *op);
typedef void (*vdm_operation_delete)(void *op,
	struct vdm_operation_output *output);

struct vdm {
	vdm_operation_new op_new;
	vdm_operation_delete op_delete;
	vdm_operation_start op_start;
	vdm_operation_check op_check;
};

struct vdm *vdm_synchronous_new(void);
void vdm_synchronous_delete(struct vdm *vdm);

/*
 * vdm_operation_impl -- the poll implementation for a generic vdm operation
 * The operation lifecycle is as follows:
 *	FUTURE_STATE_IDLE -- op_start() --> FUTURE_STATE_RUNNING
 *	FUTURE_STATE_RUNNING -- op_check() --> FUTURE_STATE_COMPLETE
 *	FUTURE_STATE_COMPLETE --> op_delete()
 */
static inline enum future_state
vdm_operation_impl(struct future_context *context, struct future_notifier *n)
{
	struct vdm_operation_data *data = future_context_get_data(context);
	struct vdm *vdm = data->vdm;

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
static inline struct vdm_operation_future
vdm_memcpy(struct vdm *vdm, void *dest, void *src, size_t n, uint64_t flags)
{
	struct vdm_operation op;
	op.type = VDM_OPERATION_MEMCPY;
	op.data.memcpy.dest = dest;
	op.data.memcpy.flags = flags;
	op.data.memcpy.n = n;
	op.data.memcpy.src = src;

	struct vdm_operation_future future = {0};
	future.data.op = vdm->op_new(vdm, &op);
	future.data.vdm = vdm;
	FUTURE_INIT(&future, vdm_operation_impl);

	return future;
}

#ifdef __cplusplus
}
#endif
#endif /* VDM_H */

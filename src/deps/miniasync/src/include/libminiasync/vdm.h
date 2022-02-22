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
	};
};

struct vdm_operation_data {
	void *op;
};

struct vdm_operation_output_memcpy {
	void *dest;
};

struct vdm_operation_output {
	enum vdm_operation_type type; /* XXX: determine if needed */
	union {
		struct vdm_operation_output_memcpy memcpy;
	};
};

FUTURE(vdm_operation_future,
	struct vdm_operation_data, struct vdm_operation_output);

struct vdm_operation_future vdm_memcpy(struct vdm *vdm, void *dest, void *src,
		size_t n, uint64_t flags);

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

#ifdef __cplusplus
}
#endif
#endif /* VDM_H */

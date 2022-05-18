// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

#include <dml/dml.h>
#include <libminiasync.h>
#include <stdbool.h>
#include <stdlib.h>

#include "core/membuf.h"
#include "core/out.h"
#include "libminiasync-vdm-dml.h"

#define SUPPORTED_FLAGS VDM_F_MEM_DURABLE | VDM_F_NO_CACHE_HINT

struct data_mover_dml {
	struct vdm base; /* must be first */
	dml_path_t path;
	struct membuf *membuf;
};

/*
 * data_mover_dml_translate_flags -- translate miniasync-vdm-dml flags
 */
static void
data_mover_dml_translate_flags(uint64_t flags, uint64_t *dml_flags)
{
	ASSERTeq((flags & ~VDM_F_VALID_FLAGS), 0);

	*dml_flags = DML_FLAG_PREFETCH_CACHE;
	for (uint64_t iflag = 1; flags > 0; iflag = iflag << 1) {
		if ((flags & iflag) == 0)
			continue;

		switch (iflag) {
			/*
			 * write to destination is identified as write to
			 * durable memory
			 */
			case VDM_F_MEM_DURABLE:
				*dml_flags |= DML_FLAG_DST1_DURABLE;
				break;
			case VDM_F_NO_CACHE_HINT:
				*dml_flags &= ~DML_FLAG_PREFETCH_CACHE;
			default: /* shouldn't be possible */
				ASSERT(0);
		}

		/* remove translated flag from the flags to be translated */
		flags = flags & (~iflag);
	}
}

/*
 * data_mover_dml_memcpy_job_init -- initializes new memcpy dml job
 */
static dml_job_t *
data_mover_dml_memcpy_job_init(dml_job_t *dml_job,
	void *dest, void *src, size_t n, uint64_t flags)
{
	uint64_t dml_flags = 0;
	data_mover_dml_translate_flags(flags, &dml_flags);

	dml_job->operation = DML_OP_MEM_MOVE;
	dml_job->source_first_ptr = (uint8_t *)src;
	dml_job->destination_first_ptr = (uint8_t *)dest;
	dml_job->source_length = n;
	dml_job->destination_length = n;
	dml_job->flags = DML_FLAG_COPY_ONLY | dml_flags;

	return dml_job;
}

/*
 * data_mover_dml_memmove_job_init -- initializes new memmove dml job
 */
static dml_job_t *
data_mover_dml_memmove_job_init(dml_job_t *dml_job,
	void *dest, void *src, size_t n, uint64_t flags)
{
	uint64_t dml_flags = 0;
	data_mover_dml_translate_flags(flags, &dml_flags);

	dml_job->operation = DML_OP_MEM_MOVE;
	dml_job->source_first_ptr = (uint8_t *)src;
	dml_job->destination_first_ptr = (uint8_t *)dest;
	dml_job->source_length = n;
	dml_job->destination_length = n;
	dml_job->flags = dml_flags;

	return dml_job;
}

/*
 * data_mover_dml_memset_job_init -- initializes new memset dml job
 */
static dml_job_t *
data_mover_dml_memset_job_init(dml_job_t *dml_job,
	void *ptr, int value, size_t n, uint64_t flags)
{
	uint64_t dml_flags = 0;
	data_mover_dml_translate_flags(flags, &dml_flags);

	dml_job->operation = DML_OP_FILL;
	dml_job->destination_first_ptr = (uint8_t *)ptr;
	dml_job->destination_length = n;
	dml_job->flags = dml_flags;

	/*
	 * Original 'memset' implementation converts the provided 'value' into
	 * a 'unsigned char' type.
	 */
	unsigned char c = (unsigned char)value;

	/* Populate the pattern */
	for (size_t i = 0; i < 8 && i < n; i++) {
		dml_job->pattern[i] = (uint8_t)c;
	}

	return dml_job;
}

/*
 * data_mover_dml_job_delete -- delete job struct
 */
static void
data_mover_dml_job_delete(dml_job_t **dml_job)
{
	dml_finalize_job(*dml_job);
}

/*
 * data_mover_dml_memory_op_job_submit -- submit job for memory operations,
 * which include memcpy and memmove (nonblocking)
 */
static void *
data_mover_dml_memory_op_job_submit(dml_job_t *dml_job)
{
	dml_status_t status;
	status = dml_submit_job(dml_job);

	return status == DML_STATUS_OK ? dml_job->destination_first_ptr : NULL;
}

/*
 * data_mover_dml_operation_new -- create a new DML job
 */
static void *
data_mover_dml_operation_new(struct vdm *vdm,
	const enum vdm_operation_type type)
{
	struct data_mover_dml *vdm_dml = (struct data_mover_dml *)vdm;
	dml_status_t status;
	uint32_t job_size;
	dml_job_t *dml_job;

	switch (type) {
		case VDM_OPERATION_MEMCPY:
		case VDM_OPERATION_MEMMOVE:
		case VDM_OPERATION_MEMSET:
			break;
		default:
			ASSERT(0); /* unreachable */
	}

	status = dml_get_job_size(vdm_dml->path, &job_size);
	if (status != DML_STATUS_OK)
		return NULL;

	dml_job = membuf_alloc(vdm_dml->membuf, job_size);
	if (dml_job == NULL)
		return NULL;

	dml_status_t job_status =
		dml_init_job(vdm_dml->path, dml_job);
	if (job_status != DML_STATUS_OK) {
		membuf_free(dml_job);
		return NULL;
	}

	return dml_job;
}

/*
 * data_mover_dml_operation_delete -- delete a DML job
 */
static void
data_mover_dml_operation_delete(void *data,
	const struct vdm_operation *operation,
	struct vdm_operation_output *output)
{
	dml_job_t *job = (dml_job_t *)data;
	dml_status_t status = dml_check_job(job);
	switch (status) {
		case DML_STATUS_BEING_PROCESSED:
			ASSERT(0 && "dml job being deleted during processing");
		case DML_STATUS_JOB_CORRUPTED:
			output->result = VDM_ERROR_JOB_CORRUPTED;
			break;
		case DML_STATUS_OK:
			output->result = VDM_SUCCESS;
			break;
		default:
			ASSERT(0);
	}

	switch (job->operation) {
		case DML_OP_MEM_MOVE:
			if (job->flags & DML_FLAG_COPY_ONLY) {
				output->type = VDM_OPERATION_MEMCPY;
				output->output.memcpy.dest =
					job->destination_first_ptr;
			} else {
				output->type = VDM_OPERATION_MEMMOVE;
				output->output.memmove.dest =
					job->destination_first_ptr;
			}
			break;
		case DML_OP_FILL:
			output->type = VDM_OPERATION_MEMSET;
			output->output.memset.str = job->destination_first_ptr;
			break;
		default:
			ASSERT(0);
	}

	data_mover_dml_job_delete(&job);

	membuf_free(data);
}

/*
 * data_mover_dml_operation_check -- checks the status of a DML job
 */
enum future_state
data_mover_dml_operation_check(void *data,
	const struct vdm_operation *operation)
{
	SUPPRESS_UNUSED(operation);

	dml_job_t *job = (dml_job_t *)data;

	dml_status_t status = dml_check_job(job);
	switch (status) {
		case DML_STATUS_BEING_PROCESSED:
			return FUTURE_STATE_RUNNING;
		case DML_STATUS_JOB_CORRUPTED:
		case DML_STATUS_OK:
			return FUTURE_STATE_COMPLETE;
		default:
			ASSERT(0);
	}
}

/*
 * data_mover_dml_operation_start -- start ('submit') asynchronous dml job
 */
int
data_mover_dml_operation_start(void *data,
	const struct vdm_operation *operation, struct future_notifier *n)
{
	if (n) {
		n->notifier_used = FUTURE_NOTIFIER_NONE;
	}

	dml_job_t *job = (dml_job_t *)data;

	switch (operation->type) {
		case VDM_OPERATION_MEMCPY:
				data_mover_dml_memcpy_job_init(job,
					operation->data.memcpy.dest,
					operation->data.memcpy.src,
					operation->data.memcpy.n,
					operation->data.memcpy.flags);
				data_mover_dml_memory_op_job_submit(job);
				break;
		case VDM_OPERATION_MEMMOVE:
				data_mover_dml_memmove_job_init(job,
					operation->data.memmove.dest,
					operation->data.memmove.src,
					operation->data.memmove.n,
					operation->data.memmove.flags);
				data_mover_dml_memory_op_job_submit(job);
			break;
		case VDM_OPERATION_MEMSET:
				data_mover_dml_memset_job_init(job,
					operation->data.memset.str,
					operation->data.memset.c,
					operation->data.memset.n,
					operation->data.memset.flags);
				data_mover_dml_memory_op_job_submit(job);
			break;
		default:
			ASSERT(0);
	}

	return 0;
}

/*
 * data_mover_dml_vdm -- dml asynchronous memcpy
 */
static struct vdm data_mover_dml_vdm = {
	.op_new = data_mover_dml_operation_new,
	.op_delete = data_mover_dml_operation_delete,
	.op_check = data_mover_dml_operation_check,
	.op_start = data_mover_dml_operation_start,
	.capabilities = SUPPORTED_FLAGS,
};

/*
 * data_mover_dml_new -- creates a new dml-based data mover instance
 */
struct data_mover_dml *
data_mover_dml_new(enum data_mover_dml_type type)
{
	struct data_mover_dml *vdm_dml = malloc(sizeof(struct data_mover_dml));
	if (vdm_dml == NULL)
		return NULL;

	vdm_dml->membuf = membuf_new(vdm_dml);
	vdm_dml->base = data_mover_dml_vdm;
	switch (type) {
		case DATA_MOVER_DML_HARDWARE:
			vdm_dml->path = DML_PATH_HW;
		break;
		case DATA_MOVER_DML_SOFTWARE:
			vdm_dml->path = DML_PATH_SW;
		break;
		case DATA_MOVER_DML_AUTO:
			vdm_dml->path = DML_PATH_AUTO;
		break;
		default:
			ASSERT(0);
	}

	return vdm_dml;
}

/*
 * data_mover_dml_get_vdm -- returns the vdm for dml data mover
 */
struct vdm *
data_mover_dml_get_vdm(struct data_mover_dml *dmd)
{
	return &dmd->base;
}

/*
 * data_mover_dml_delete -- deletes a vdm_dml instance
 */
void
data_mover_dml_delete(struct data_mover_dml *dmd)
{
	membuf_delete(dmd->membuf);
	free(dmd);
}

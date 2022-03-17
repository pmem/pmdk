// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

#include <dml/dml.h>
#include <libminiasync.h>
#include <stdbool.h>
#include <stdlib.h>

#include "core/membuf.h"
#include "libminiasync-vdm-dml.h"
#include "core/out.h"
#include "core/util.h"

struct data_mover_dml {
	struct vdm base; /* must be first */

	struct membuf *membuf;
};

/*
 * data_mover_dml_translate_flags -- translate miniasync-vdm-dml flags
 */
static void
data_mover_dml_translate_flags(uint64_t flags, uint64_t *dml_flags,
	dml_path_t *path)
{
	ASSERTeq((flags & ~MINIASYNC_DML_F_VALID_FLAGS), 0);

	*dml_flags = 0;
	*path = DML_PATH_SW; /* default path */
	for (uint64_t iflag = 1; flags > 0; iflag = iflag << 1) {
		if ((flags & iflag) == 0)
			continue;

		switch (iflag) {
			/*
			 * write to destination is identified as write to
			 * durable memory
			 */
			case MINIASYNC_DML_F_MEM_DURABLE:
				*dml_flags |= DML_FLAG_DST1_DURABLE;
				break;
			/* perform operation using hardware path */
			case MINIASYNC_DML_F_PATH_HW:
				*path = DML_PATH_HW;
				break;
			default: /* shouldn't be possible */
				ASSERT(0);
		}

		/* remove translated flag from the flags to be translated */
		flags = flags & (~iflag);
	}
}

/*
 * data_mover_dml_memcpy_job_new -- create a new memcpy job struct
 */
static dml_job_t *
data_mover_dml_memcpy_job_new(struct data_mover_dml *vdm_dml,
	void *dest, void *src, size_t n, uint64_t flags)
{
	dml_status_t status;
	uint32_t job_size;
	dml_job_t *dml_job = NULL;

	uint64_t dml_flags = 0;
	dml_path_t path;
	data_mover_dml_translate_flags(flags, &dml_flags, &path);

	status = dml_get_job_size(path, &job_size);
	ASSERTeq(status, DML_STATUS_OK);

	dml_job = (dml_job_t *)membuf_alloc(vdm_dml->membuf, job_size);

	status = dml_init_job(path, dml_job);
	ASSERTeq(status, DML_STATUS_OK);

	dml_job->operation = DML_OP_MEM_MOVE;
	dml_job->source_first_ptr = (uint8_t *)src;
	dml_job->destination_first_ptr = (uint8_t *)dest;
	dml_job->source_length = n;
	dml_job->destination_length = n;
	dml_job->flags = DML_FLAG_COPY_ONLY | dml_flags;

	return dml_job;
}

/*
 * data_mover_dml_memcpy_job_delete -- delete job struct
 */
static void
data_mover_dml_memcpy_job_delete(dml_job_t **dml_job)
{
	dml_finalize_job(*dml_job);
}

/*
 * data_mover_dml_memcpy_job_submit -- submit memcpy job (nonblocking)
 */
static void *
data_mover_dml_memcpy_job_submit(dml_job_t *dml_job)
{
	dml_status_t status;
	status = dml_submit_job(dml_job);
	ASSERTeq(status, DML_STATUS_OK);

	return dml_job->destination_first_ptr;
}

/*
 * data_mover_dml_operation_new -- create a new DML job
 */
static void *
data_mover_dml_operation_new(struct vdm *vdm,
	const struct vdm_operation *operation)
{
	struct data_mover_dml *vdm_dml = (struct data_mover_dml *)vdm;

	switch (operation->type) {
		case VDM_OPERATION_MEMCPY:
		return data_mover_dml_memcpy_job_new(vdm_dml,
			operation->data.memcpy.dest,
			operation->data.memcpy.src,
			operation->data.memcpy.n,
			operation->data.memcpy.flags);
		default:
		ASSERT(0); /* unreachable */
	}
	return NULL;
}

/*
 * data_mover_dml_operation_delete -- delete a DML job
 */
static void
data_mover_dml_operation_delete(void *op, struct vdm_operation_output *output)
{
	dml_job_t *job = (dml_job_t *)op;

	switch (job->operation) {
		case DML_OP_MEM_MOVE:
			output->type = VDM_OPERATION_MEMCPY;
			output->output.memcpy.dest =
				job->destination_first_ptr;
			break;
		default:
			ASSERT(0);
	}

	data_mover_dml_memcpy_job_delete(&job);

	membuf_free(op);
}

/*
 * data_mover_dml_operation_check -- checks the status of a DML job
 */
enum future_state
data_mover_dml_operation_check(void *op)
{
	dml_job_t *job = (dml_job_t *)op;

	dml_status_t status = dml_check_job(job);
	ASSERTne(status, DML_STATUS_JOB_CORRUPTED);

	return (status == DML_STATUS_OK) ?
		FUTURE_STATE_COMPLETE : FUTURE_STATE_RUNNING;
}

/*
 * data_mover_dml_operation_start -- start ('submit') asynchronous dml job
 */
int
data_mover_dml_operation_start(void *op, struct future_notifier *n)
{
	n->notifier_used = FUTURE_NOTIFIER_NONE;

	dml_job_t *job = (dml_job_t *)op;

	data_mover_dml_memcpy_job_submit(job);

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
};

/*
 * data_mover_dml_new -- creates a new dml-based data mover instance
 */
struct data_mover_dml *
data_mover_dml_new(void)
{
	struct data_mover_dml *vdm_dml = malloc(sizeof(struct data_mover_dml));
	if (vdm_dml == NULL)
		return NULL;

	vdm_dml->membuf = membuf_new(vdm_dml);
	vdm_dml->base = data_mover_dml_vdm;

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

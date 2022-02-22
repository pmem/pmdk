// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include <dml/dml.h>

#include "cpu.h"

/*
 * util_dml_check_hw_available -- check DML operations with hardware path can
 *                                be performed on current platform. Returns '0'
 *                                when it's available and '-1' when it's not.
 */
int
util_dml_check_hw_available()
{
	if (!is_cpu_movdir64b_present()) {
		return -1;
	}

	int ret = 0;
	size_t buf_alloc_size = sizeof(char);
	char *buf = malloc(buf_alloc_size);
	char *dst = malloc(buf_alloc_size);

	dml_status_t status;
	uint32_t job_size;
	dml_job_t *dml_job_ptr;

	status = dml_get_job_size(DML_PATH_HW, &job_size);
	if (status != DML_STATUS_OK) {
		ret = -1;
		goto free_bufs;
	}

	dml_job_ptr = (dml_job_t *)malloc(job_size);

	status = dml_init_job(DML_PATH_HW, dml_job_ptr);
	if (status != DML_STATUS_OK) {
		ret = -1;
		goto free_job_ptr;
	}

	dml_job_ptr->operation = DML_OP_MEM_MOVE;
	dml_job_ptr->source_first_ptr = (uint8_t *)buf;
	dml_job_ptr->destination_first_ptr = (uint8_t *)dst;
	dml_job_ptr->source_length = buf_alloc_size;
	dml_job_ptr->destination_length = buf_alloc_size;

	status = dml_execute_job(dml_job_ptr);
	if (status != DML_STATUS_OK) {
		ret = -1;
	}

	status = dml_finalize_job(dml_job_ptr);
	assert(status == DML_STATUS_OK);
free_job_ptr:
	free(dml_job_ptr);
free_bufs:
	free(dst);
	free(buf);

	return ret;
}

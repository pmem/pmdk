// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <libminiasync.h>
#include <libminiasync-vdm-dml.h>
#include "util_dml.h"
#include "test_helpers.h"

static int
dml_memmove(enum data_mover_dml_type type, uint64_t flags, size_t size)
{
	/* assert that size is a multiply of 4 */
	if (size % 4 != 0) {
		fprintf(stderr,
				"size parameter is not a multiply of 4");
		return 1;
	}

	char *buffer = malloc(size);
	if (buffer == NULL) {
		fprintf(stderr,
				"memory for the buffer could not be allocated");
		return 1;
	}

	/* fill up the buffer with 'As', 'Bs' and 'Cs' */
	memset(buffer, 'A', size / 4);
	memset(buffer + size / 4, 'B', size / 4);
	memset(buffer + 2 * size / 4, 'C', size / 4);

	struct runtime *r = runtime_new();

	struct data_mover_dml *dmd = data_mover_dml_new(type);
	struct vdm *dml_mover_async = data_mover_dml_get_vdm(dmd);

	/*
	 * Copy 'As' and 'Bs' from the beginning of the buffer
	 * to the address where 'Bs' start.
	 */
	struct vdm_operation_future test_memmove_fut =
		vdm_memmove(dml_mover_async, buffer + size / 4,
					buffer, 2 * size / 4, 0);

	runtime_wait(r, FUTURE_AS_RUNNABLE(&test_memmove_fut));

	for (size_t i = 0; i < 2 * size / 4; i++) {
		UT_ASSERTeq(buffer[i], 'A');
	}

	for (size_t i = 2 * size / 4; i < 3 * size / 4; i++) {
		UT_ASSERTeq(buffer[i], 'B');
	}

	data_mover_dml_delete(dmd);

	runtime_delete(r);
	free(buffer);

	return 0;
}

static int
test_dml_basic_memmove()
{
	return
		dml_memmove(DATA_MOVER_DML_SOFTWARE, 0, 4) ||
		dml_memmove(DATA_MOVER_DML_SOFTWARE, 0, 12) ||
		dml_memmove(DATA_MOVER_DML_SOFTWARE, 0, 1024);
}

static int
test_dml_durable_flag_memmove()
{
	return
		dml_memmove(DATA_MOVER_DML_SOFTWARE,
					VDM_F_MEM_DURABLE, 4) ||
		dml_memmove(DATA_MOVER_DML_SOFTWARE,
					VDM_F_MEM_DURABLE, 12) ||
		dml_memmove(DATA_MOVER_DML_SOFTWARE,
					VDM_F_MEM_DURABLE, 1024);
}

static int
test_dml_hw_path_flag_memmove()
{
	return
		dml_memmove(DATA_MOVER_DML_HARDWARE, 0, 4) ||
		dml_memmove(DATA_MOVER_DML_HARDWARE, 0, 12) ||
		dml_memmove(DATA_MOVER_DML_HARDWARE, 0, 1024);
}

int
main(void)
{
	int ret = test_dml_basic_memmove();
	if (ret)
		return ret;

	ret = test_dml_durable_flag_memmove();
	if (ret)
		return ret;

	if (util_dml_check_hw_available() == 0) {
		ret = test_dml_hw_path_flag_memmove();
		if (ret)
			return ret;
	} else {
		UT_LOG_SKIP("test_dml_hw_path_flag_memmove");
	}

	return 0;
}

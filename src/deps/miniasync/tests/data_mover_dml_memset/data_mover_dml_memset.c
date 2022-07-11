// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <libminiasync.h>
#include <libminiasync-vdm-dml.h>
#include "test_helpers.h"
#include "util_dml.h"

static int
dml_memset(enum data_mover_dml_type type, uint64_t flags, size_t size)
{
	/* Size should be dividable by 2 */
	char *buf = malloc(size);

	struct runtime *r = runtime_new();

	struct data_mover_dml *dmd = data_mover_dml_new(type);
	struct vdm *dml_mover_async = data_mover_dml_get_vdm(dmd);

	/* First set the whole buffer with 'x' values */
	struct vdm_operation_future set_x_fut =
			vdm_memset(dml_mover_async, buf, 'x', size, flags);

	runtime_wait(r, FUTURE_AS_RUNNABLE(&set_x_fut));

	for (size_t i = 0; i < size; i++) {
		UT_ASSERTeq(buf[i], 'x');
	}

	/* Then, set the buffer with 'y', and 'z' values in a 1:1 ratio */
	struct vdm_operation_future set_y_fut =
			vdm_memset(dml_mover_async, buf, 'y', size / 2, flags);
	struct vdm_operation_future set_z_fut =
			vdm_memset(dml_mover_async, buf + size / 2, 'z',
					size / 2, flags);

	struct future *futs[] = {
		FUTURE_AS_RUNNABLE(&set_y_fut),
		FUTURE_AS_RUNNABLE(&set_z_fut)
	};
	runtime_wait_multiple(r, futs, 2);

	for (size_t i = 0; i < size / 2; i++) {
		UT_ASSERTeq(buf[i], 'y');
		UT_ASSERTeq(buf[i + size / 2], 'z');
	}

	data_mover_dml_delete(dmd);

	runtime_delete(r);
	free(buf);

	return 0;
}

static int
test_dml_basic_memset()
{
	return
		dml_memset(DATA_MOVER_DML_SOFTWARE, 0, 8) ||
		dml_memset(DATA_MOVER_DML_SOFTWARE, 0, 16) ||
		dml_memset(DATA_MOVER_DML_SOFTWARE, 0, 32) ||
		dml_memset(DATA_MOVER_DML_SOFTWARE, 0, 64) ||
		dml_memset(DATA_MOVER_DML_SOFTWARE, 0, 128) ||
		dml_memset(DATA_MOVER_DML_SOFTWARE, 0, 256) ||
		dml_memset(DATA_MOVER_DML_SOFTWARE, 0, 512) ||
		dml_memset(DATA_MOVER_DML_SOFTWARE, 0, 1024);
}

static int
test_dml_durable_flag_memset()
{
	return
		dml_memset(DATA_MOVER_DML_SOFTWARE, VDM_F_MEM_DURABLE, 8) ||
		dml_memset(DATA_MOVER_DML_SOFTWARE, VDM_F_MEM_DURABLE, 16) ||
		dml_memset(DATA_MOVER_DML_SOFTWARE, VDM_F_MEM_DURABLE, 32) ||
		dml_memset(DATA_MOVER_DML_SOFTWARE, VDM_F_MEM_DURABLE, 64) ||
		dml_memset(DATA_MOVER_DML_SOFTWARE, VDM_F_MEM_DURABLE, 128) ||
		dml_memset(DATA_MOVER_DML_SOFTWARE, VDM_F_MEM_DURABLE, 256) ||
		dml_memset(DATA_MOVER_DML_SOFTWARE, VDM_F_MEM_DURABLE, 512) ||
		dml_memset(DATA_MOVER_DML_SOFTWARE, VDM_F_MEM_DURABLE, 1024);
}

static int
test_dml_hw_path_flag_memset()
{
	return
		dml_memset(DATA_MOVER_DML_HARDWARE, 0, 8) ||
		dml_memset(DATA_MOVER_DML_HARDWARE, 0, 16) ||
		dml_memset(DATA_MOVER_DML_HARDWARE, 0, 32) ||
		dml_memset(DATA_MOVER_DML_HARDWARE, 0, 64) ||
		dml_memset(DATA_MOVER_DML_HARDWARE, 0, 128) ||
		dml_memset(DATA_MOVER_DML_HARDWARE, 0, 256) ||
		dml_memset(DATA_MOVER_DML_HARDWARE, 0, 512) ||
		dml_memset(DATA_MOVER_DML_HARDWARE, 0, 1024);
}

int
main(void)
{
	int ret = test_dml_basic_memset();
	if (ret)
		return ret;

	ret = test_dml_durable_flag_memset();
	if (ret)
		return ret;

	if (util_dml_check_hw_available() == 0) {
		ret = test_dml_hw_path_flag_memset();
		if (ret)
			return ret;
	} else {
		UT_LOG_SKIP("test_dml_hw_path_flag_memset");
	}

	return 0;
}

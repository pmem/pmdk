// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <libminiasync.h>
#include <libminiasync-vdm-dml.h>
#include "test_helpers.h"
#include "util_dml.h"

#define ARRAY_SIZE 50

static int
dml_flush(enum data_mover_dml_type type, uint64_t flags)
{
	struct runtime *r = runtime_new();

	struct data_mover_dml *dmd = data_mover_dml_new(type);
	struct vdm *dml_mover_async = data_mover_dml_get_vdm(dmd);

	uint8_t destination_array[ARRAY_SIZE];

	struct vdm_operation_future a_to_b = vdm_flush(dml_mover_async,
			destination_array, ARRAY_SIZE, flags);

	runtime_wait(r, FUTURE_AS_RUNNABLE(&a_to_b));

	data_mover_dml_delete(dmd);

	runtime_delete(r);

	return 0;
}

static int
test_dml_basic_flush()
{
	return dml_flush(DATA_MOVER_DML_SOFTWARE, 0);
}

static int
test_dml_hw_path_flag_flush()
{
	return dml_flush(DATA_MOVER_DML_HARDWARE, 0);
}

int
main(void)
{
	int ret = test_dml_basic_flush();
	if (ret)
		return ret;
	if (util_dml_check_hw_available() == 0) {
		ret = test_dml_hw_path_flag_flush();
		if (ret)
			return ret;
	} else {
		UT_LOG_SKIP("test_dml_hw_path_flag_flush");
	}

	return 0;
}

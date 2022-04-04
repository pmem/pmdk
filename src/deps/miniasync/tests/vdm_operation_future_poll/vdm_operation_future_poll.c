// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#include <libminiasync.h>
#include <libminiasync-vdm-dml.h>
#include <string.h>
#include "util_dml.h"

/*
 * Test if future created by vdm_memcpy and instance of a mover is
 * safe to poll with simple future_poll without any notifier
 */
int
test_vdm_future_poll(struct vdm *vdm, size_t n)
{
	char *src = malloc(n);
	char *dst = malloc(n);
	memset(src, 5, n);

	struct vdm_operation_future fut = vdm_memcpy(vdm, dst, src, n, 0);
	FUTURE_BUSY_POLL(&fut);

	free(src);
	free(dst);
	return 0;
}

int
main(void)
{
	int ret = 0;
	size_t size = 512;

	struct data_mover_sync *dms = data_mover_sync_new();
	ret = test_vdm_future_poll(data_mover_sync_get_vdm(dms), size);
	data_mover_sync_delete(dms);

	struct data_mover_threads *dmt = data_mover_threads_default();
	ret = test_vdm_future_poll(data_mover_threads_get_vdm(dmt), size);
	data_mover_threads_delete(dmt);

	struct data_mover_dml *dmd = data_mover_dml_new(DATA_MOVER_DML_AUTO);
	ret = test_vdm_future_poll(data_mover_dml_get_vdm(dmd), size);
	data_mover_dml_delete(dmd);

	return ret;
}

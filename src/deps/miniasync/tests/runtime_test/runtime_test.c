// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#include <libminiasync.h>
#include <libminiasync-vdm-dml.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include "util_dml.h"
#include "core/os.h"
#include "test_helpers.h"

/*
 * test_runtime_wait_multiple -- test executing multiple futures
 * with different data movers.
 */
int
test_runtime_wait_multiple(size_t str_len)
{
	int ret = 0;
	unsigned seed = (unsigned)time(NULL);
	fprintf(stdout, "seed: %u\n", seed);
	struct runtime *r = runtime_new();

	struct data_mover_sync *dms = data_mover_sync_new();
	struct data_mover_threads *dmt = data_mover_threads_default();
	struct data_mover_dml *dmd = data_mover_dml_new(DATA_MOVER_DML_AUTO);

	struct vdm *vdm_sync = data_mover_sync_get_vdm(dms);
	struct vdm *vdm_threads = data_mover_threads_get_vdm(dmt);
	struct vdm *vdm_dml = data_mover_dml_get_vdm(dmd);

	char *buf_a = malloc(str_len);
	char *buf_b = malloc(str_len);

	if (!buf_a || !buf_b)
		UT_FATAL("buffers out of memory");

	char symbol_first = (char)os_rand_r(&seed) % 256;
	char symbol_second = (char)os_rand_r(&seed) % 256;

	struct future **futures = malloc(sizeof(struct future *) * 4);
	if (futures == NULL)
		UT_FATAL("futures out of memory");

	struct vdm_operation_future memset_fut_symbol_f_to_a =
		vdm_memset(vdm_threads, buf_a, symbol_first, str_len / 2, 0);

	struct vdm_operation_future memset_fut_symbol_s_to_a =
		vdm_memset(vdm_sync, buf_a + str_len / 2, symbol_second,
			str_len / 2, 0);

	struct vdm_operation_future memset_fut_symbol_s_to_b =
		vdm_memset(vdm_dml, buf_b, symbol_second, str_len / 2, 0);

	struct vdm_operation_future memset_fut_symbol_f_to_b =
		vdm_memset(vdm_sync, buf_b + str_len / 2, symbol_first,
			str_len / 2, 0);

	futures[0] = FUTURE_AS_RUNNABLE(&memset_fut_symbol_f_to_a);
	futures[1] = FUTURE_AS_RUNNABLE(&memset_fut_symbol_s_to_a);
	futures[2] = FUTURE_AS_RUNNABLE(&memset_fut_symbol_s_to_b);
	futures[3] = FUTURE_AS_RUNNABLE(&memset_fut_symbol_f_to_b);

	runtime_wait_multiple(r, futures, 4);

	for (size_t i = 0; i < str_len / 2; i++) {
		UT_ASSERTeq(buf_a[i], symbol_first);
		UT_ASSERTeq(buf_b[i], symbol_second);
	}

	for (size_t i = str_len / 2; i < str_len; i++) {
		UT_ASSERTeq(buf_a[i], symbol_second);
		UT_ASSERTeq(buf_b[i], symbol_first);
	}

	free(buf_a);
	free(buf_b);
	free(futures);

	runtime_delete(r);
	data_mover_sync_delete(dms);
	data_mover_threads_delete(dmt);
	data_mover_dml_delete(dmd);

	return ret;
}

int
main(void)
{
	int ret = 0;
	size_t size = 512;

	ret = test_runtime_wait_multiple(size);

	return ret;
}

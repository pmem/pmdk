// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "libminiasync.h"
#include "core/os.h"
#include "test_helpers.h"

int
test_thread_memset_single(size_t str_len)
{
	int ret = 0;
	unsigned seed = (unsigned)time(NULL);
	fprintf(stdout, "seed: %u\n", seed);
	struct runtime *r = runtime_new();
	struct data_mover_threads *dmt = data_mover_threads_default();

	if (dmt == NULL) {
		runtime_delete(r);
		return 1;
	}

	struct vdm *vdm = data_mover_threads_get_vdm(dmt);

	char *buf_a = malloc(str_len);

	if (buf_a == NULL)
		UT_FATAL("buffer out of memory");

	char symbol = (char)os_rand_r(&seed) % 256;

	struct vdm_operation_future test_memset_fut =
		vdm_memset(vdm, buf_a, symbol, str_len, 0);

	struct future *future_memset = FUTURE_AS_RUNNABLE(&test_memset_fut);

	runtime_wait(r, future_memset);

	for (size_t i = 0; i < str_len; i++) {
		UT_ASSERTeq(buf_a[i], symbol);
	}

	free(buf_a);

	runtime_delete(r);
	data_mover_threads_delete(dmt);

	return ret;
}

int
test_thread_memset_multiple(size_t str_len)
{
	int ret = 0;
	unsigned seed = (unsigned)time(NULL);
	fprintf(stdout, "seed: %u\n", seed);
	struct runtime *r = runtime_new();
	struct data_mover_threads *dmt = data_mover_threads_default();

	if (dmt == NULL) {
		fprintf(stderr,
				"error while creating synchronous data mover");
		runtime_delete(r);
		return 1;
	}

	struct vdm *vdm = data_mover_threads_get_vdm(dmt);

	char *buf_a = malloc(str_len);
	char *buf_b = malloc(str_len);

	if (!buf_a || !buf_b)
		UT_FATAL("buffers out of memory");

	char symbol_first = (char)os_rand_r(&seed) % 256;
	char symbol_second = (char)os_rand_r(&seed) % 256;

	struct future **futures = malloc(sizeof(struct future *) * 4);
	if (futures == NULL)
		UT_FATAL("futures out of memory");

	struct vdm_operation_future memmove_fut_symbol_f_to_a =
		vdm_memset(vdm, buf_a, symbol_first, str_len / 2, 0);

	struct vdm_operation_future memmove_fut_symbol_s_to_a =
		vdm_memset(vdm, buf_a + str_len / 2, symbol_second,
			str_len / 2, 0);

	struct vdm_operation_future memmove_fut_symbol_s_to_b =
		vdm_memset(vdm, buf_b, symbol_second, str_len / 2, 0);

	struct vdm_operation_future memmove_fut_symbol_f_to_b =
		vdm_memset(vdm, buf_b + str_len / 2, symbol_first,
			str_len / 2, 0);

	futures[0] = FUTURE_AS_RUNNABLE(&memmove_fut_symbol_f_to_a);
	futures[1] = FUTURE_AS_RUNNABLE(&memmove_fut_symbol_s_to_a);
	futures[2] = FUTURE_AS_RUNNABLE(&memmove_fut_symbol_s_to_b);
	futures[3] = FUTURE_AS_RUNNABLE(&memmove_fut_symbol_f_to_b);

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
	data_mover_threads_delete(dmt);

	return ret;
}

int
main(void)
{
	return
		test_thread_memset_single(100000000) ||
		test_thread_memset_single(300000000) ||
		test_thread_memset_single(500000000) ||
		test_thread_memset_multiple(100000000) ||
		test_thread_memset_multiple(300000000) ||
		test_thread_memset_multiple(500000000);
}

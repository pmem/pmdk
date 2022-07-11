// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "libminiasync.h"
#include "core/os.h"
#include "test_helpers.h"

int
test_thread_memmove_single(size_t str_len)
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
	char *buf_b = malloc(str_len);

	if (!buf_a || !buf_b)
		UT_FATAL("buffers out of memory");

	for (size_t i = 0; i < str_len; i++) {
		buf_a[i] = (char)os_rand_r(&seed) % 256;
		buf_b[i] = (char)os_rand_r(&seed) % 256;
	}

	if (strcmp(buf_a, buf_b) == 0) {
		UT_FATAL("buffers contain the same value before memmove");
	}

	struct vdm_operation_future test_memmove_fut =
		vdm_memmove(vdm, buf_a, buf_b, str_len, 0);

	struct future *future_memmove = FUTURE_AS_RUNNABLE(&test_memmove_fut);

	runtime_wait(r, future_memmove);

	for (size_t i = 0; i < str_len; i++) {
		UT_ASSERTeq(buf_a[i], buf_b[i]);
	}

	free(buf_a);
	free(buf_b);

	runtime_delete(r);
	data_mover_threads_delete(dmt);

	return ret;
}

int
test_thread_memmove_multiple(size_t str_len)
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
	char *buf_c = malloc(str_len);
	char *buf_d = malloc(str_len);

	if (!buf_a || !buf_b || !buf_c || !buf_d)
		UT_FATAL("buffers out of memory");

	for (size_t i = 0; i < str_len; i++) {
		buf_a[i] = (char)os_rand_r(&seed) % 256;
		buf_b[i] = (char)os_rand_r(&seed) % 256;
		buf_c[i] = (char)os_rand_r(&seed) % 256;
		buf_d[i] = (char)os_rand_r(&seed) % 256;
	}

	struct future **futures = malloc(sizeof(struct future *) * 4);
	if (futures == NULL)
		UT_FATAL("futures out of memory");

	struct vdm_operation_future memmove_fut_b_to_a =
		vdm_memmove(vdm, buf_a, buf_b, str_len / 2, 0);

	struct vdm_operation_future memmove_fut_c_to_a =
		vdm_memmove(vdm, buf_a + str_len / 2, buf_c,
			str_len / 2, 0);

	struct vdm_operation_future memmove_fut_b_to_d =
		vdm_memmove(vdm, buf_d, buf_b, str_len / 2, 0);

	struct vdm_operation_future memmove_fut_c_to_d =
		vdm_memmove(vdm, buf_d + str_len / 2, buf_c,
			str_len / 2, 0);

	futures[0] = FUTURE_AS_RUNNABLE(&memmove_fut_b_to_a);
	futures[1] = FUTURE_AS_RUNNABLE(&memmove_fut_c_to_a);
	futures[2] = FUTURE_AS_RUNNABLE(&memmove_fut_b_to_d);
	futures[3] = FUTURE_AS_RUNNABLE(&memmove_fut_c_to_d);

	runtime_wait_multiple(r, futures, 4);

	for (size_t i = 0; i < str_len / 2; i++) {
		UT_ASSERTeq(buf_a[i], buf_b[i]);
		UT_ASSERTeq(buf_d[i], buf_b[i]);
	}

	size_t j = 0;
	for (size_t i = str_len / 2; i < str_len; i++) {
		UT_ASSERTeq(buf_a[i], buf_c[j]);
		UT_ASSERTeq(buf_d[i], buf_c[j]);
		j++;
	}

	free(buf_a);
	free(buf_b);
	free(buf_c);
	free(buf_d);
	free(futures);

	runtime_delete(r);
	data_mover_threads_delete(dmt);

	return ret;
}

/*
 * test_supported_flags -- test if data_mover_threads support correct flags
 */
int test_supported_flags() {
	struct data_mover_threads *dmt = data_mover_threads_default();
	if (dmt == NULL) {
		fprintf(stderr,
				"error while creating threads data mover");
		return 1;
	}
	struct vdm *thread_mover = data_mover_threads_get_vdm(dmt);
	int ret = test_flag(thread_mover, VDM_F_MEM_DURABLE, 0);
	data_mover_threads_delete(dmt);

	return ret;
}

int
main(void)
{
	return
		test_thread_memmove_single(10000000) ||
		test_thread_memmove_single(30000000) ||
		test_thread_memmove_single(50000000) ||
		test_thread_memmove_multiple(10000000) ||
		test_thread_memmove_multiple(30000000) ||
		test_thread_memmove_multiple(50000000) ||
		test_supported_flags();
}

// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#include <stdlib.h>
#include <string.h>
#include "libminiasync.h"
#include "libminiasync/data_mover_threads.h"

#define TEST_SIZE 1024

int src_dst_new(char **src1, char **dst1, char **src2, char **dst2) {
	size_t size = TEST_SIZE * sizeof(char);
	*src1 = malloc(size);
	if (*src1 == NULL)
		return 1;

	*dst1 = malloc(size);
	if (*dst1 == NULL)
		goto end_1;

	*src2 = malloc(size * 2);
	if (*src2 == NULL)
		goto end_2;

	*dst2 = malloc(size * 2);
	if (*dst2 == NULL)
		goto end_3;

	return 0;

end_3:
	free(*src2);
end_2:
	free(*dst1);
end_1:
	free(*src1);

	return 1;
}

void src_dst_del(char **src1, char **dst1, char **src2, char **dst2) {
	free(*src1);
	free(*src2);
	free(*dst1);
	free(*dst2);
}

int
main(void)
{
	int ret = 0;
	/*
	 * First, we have to create a runner instance, get descriptor for
	 * asynchronous memcpy which is vdm_descriptor_threads and
	 * create a vdm instance using this descriptor.
	 */
	struct runtime *r = runtime_new();

	struct data_mover_threads *dmt = data_mover_threads_default();
	if (dmt == NULL) {
			fprintf(stderr, "Failed to allocate data mover.\n");
			runtime_delete(r);
			return 1;
	}
	struct vdm *vdm = data_mover_threads_get_vdm(dmt);

	/*
	 * Asynchronous memcpy operations can be performed by a runtime one
	 * after another. In this example we call two unique memcpy
	 * operations in a loop.
	 */
	char *src1;
	char *dst1;
	char *src2;
	char *dst2;

	for (int i = 0; i < 2; i++) {
		ret = src_dst_new(&src1, &dst1, &src2, &dst2);
		if (ret)
			goto end;

		memset(src1, 7, TEST_SIZE);
		memset(src2, 6, TEST_SIZE * 2);

		/*
		 * Create futures to be performed asynchronously.
		 */
		struct vdm_operation_future fut = vdm_memcpy(
			vdm, dst1, src1, TEST_SIZE, 0);
		struct vdm_operation_future fut2 = vdm_memcpy(
			vdm, dst2, src2, TEST_SIZE * 2, 0);

		struct future *futs[] = {FUTURE_AS_RUNNABLE(&fut),
			FUTURE_AS_RUNNABLE(&fut2)
		};

		/*
		 * Performing memcpy operations asynchronously is always
		 * finished before returning from runtime_wait_multiple, so we
		 * can safely do cleanup or use the copied data just after
		 * calling that.
		 */
		runtime_wait_multiple(r, futs, 2);

		src_dst_del(&src1, &dst1, &src2, &dst2);
	}

	/*
	 * Remember to call these after finishing memcpy operations,
	 * because creating instance of vdm mover with vdm_new and
	 * vdm_descriptor_threads creates system threads and allocates memory
	 * which is freed only by vdm_delete or at end of execution of a process
	 * that called vdm_new.
	 */
end:
	data_mover_threads_delete(dmt);
	runtime_delete(r);
	return ret;
}

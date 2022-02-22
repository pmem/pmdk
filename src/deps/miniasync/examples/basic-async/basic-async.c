// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#include <stdlib.h>
#include <string.h>
#include "libminiasync.h"
#include "libminiasync/data_mover_threads.h"

#define TEST_SIZE 1024

int
main(void)
{
	/*
	 * First, we have to create a runner instance, get descriptor for
	 * asynchronous memcpy which is vdm_descriptor_threads and
	 * create a vdm instance using this descriptor.
	 */
	struct runtime *r = runtime_new();

	struct data_mover_threads *dmt = data_mover_threads_default();
	struct vdm *vdm = data_mover_threads_get_vdm(dmt);

	/*
	 * Asynchronous memcpy operations can be performed by a runtime one
	 * after another. In this example we call two unique memcpy
	 * operations in a loop.
	 */
	for (int i = 0; i < 2; i++) {
		char *src1 = malloc(TEST_SIZE * sizeof(char));
		char *dst1 = malloc(TEST_SIZE * sizeof(char));
		char *src2 = malloc(TEST_SIZE * 2 * sizeof(char));
		char *dst2 = malloc(TEST_SIZE * 2 * sizeof(char));

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

		free(src1);
		free(src2);
		free(dst1);
		free(dst2);
	}

	/*
	 * Remember to call these after finishing memcpy operations,
	 * because creating instance of vdm mover with vdm_new and
	 * vdm_descriptor_threads creates system threads and allocates memory
	 * which is freed only by vdm_delete or at end of execution of a process
	 * that called vdm_new.
	 */
	data_mover_threads_delete(dmt);
	runtime_delete(r);
	return 0;
}

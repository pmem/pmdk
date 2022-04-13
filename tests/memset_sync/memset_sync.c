// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#include <stdlib.h>
#include <string.h>
#include "libminiasync.h"
#include "test_helpers.h"

int
test_memset(char c)
{
	int ret = 0;

	size_t buffer_size = strlen("teststring");

	char *buffer = malloc(buffer_size + 1);
	if (buffer == NULL) {
		fprintf(stderr,
				"memory for the first buffer could not be allocated");
		return 1;
	}

	char *test_buffer = malloc(buffer_size + 1);
	if (test_buffer == NULL) {
		fprintf(stderr,
				"memory for the second buffer could not be allocated");
		goto cleanup_1;
	}

	memcpy(buffer, "teststring", buffer_size + 1);
	memcpy(test_buffer, buffer, buffer_size + 1);

	struct data_mover_sync *dms = data_mover_sync_new();
	if (dms == NULL) {
		ret = 1;
		fprintf(stderr,
				"error while creating synchronous data mover");
		goto cleanup_2;
	}

	struct vdm *sync_mover = data_mover_sync_get_vdm(dms);
	if (sync_mover == NULL) {
		ret = 1;
		fprintf(stderr,
				"error while extracting synchronous vdm");
		goto cleanup_3;
	}

	struct vdm_operation_future test_memset_fut =
		vdm_memset(sync_mover, buffer, c, buffer_size / 2, 0);

	FUTURE_BUSY_POLL(&test_memset_fut);

	for (size_t i = 0; i < buffer_size / 2; i++) {
		UT_ASSERTeq(buffer[i], c);
	}

	for (size_t i = buffer_size / 2; i < buffer_size + 1; i++) {
		UT_ASSERTeq(buffer[i], test_buffer[i]);
	}

	cleanup_3:
	data_mover_sync_delete(dms);

	cleanup_2:
	free(test_buffer);

	cleanup_1:
	free(buffer);

	return ret;
}

int
main(void)
{
	return
		test_memset('!') ||
		test_memset('a') ||
		test_memset('X') ||
		test_memset((char)0xA5) ||
		test_memset((char)0x5A);
}

// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022-2023, Intel Corporation */

#include <stdlib.h>
#include <string.h>
#include "libminiasync.h"
#include "test_helpers.h"

/*
 * test_basic_memmove -- tests memmove vdm operation
 * with two, separate, not overlapping buffers.
 */
int
test_basic_memmove()
{
	int ret = 0;

	size_t buf_a_size = strlen("teststring1");
	size_t buf_b_size = strlen("teststring2");

	char *buf_a = malloc(buf_a_size + 1);
	if (buf_a == NULL) {
		fprintf(stderr,
				"memory for the first buffer could not be allocated");
		return 1;
	}

	char *buf_b = malloc(buf_b_size + 1);
	if (buf_b == NULL) {
		ret = 1;
		fprintf(stderr,
				"memory for the second buffer could not be allocated");
		goto cleanup_1;
	}

	memcpy(buf_a, "teststring1", buf_a_size + 1);
	memcpy(buf_b, "teststring2", buf_b_size + 1);

	if (strcmp(buf_a, buf_b) == 0) {
		ret = 1;
		fprintf(stderr,
				"buffers before memmove contain same values");
		goto cleanup_2;
	}

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

	struct vdm_operation_future test_memmove_fut =
		vdm_memmove(sync_mover, buf_a, buf_b, buf_a_size, 0);

	FUTURE_BUSY_POLL(&test_memmove_fut);

	ret = strcmp(buf_a, buf_b);

	if (ret)
		fprintf(stderr,
				"buffers after memmove do not contain same values");

	cleanup_3:
	data_mover_sync_delete(dms);

	cleanup_2:
	free(buf_b);

	cleanup_1:
	free(buf_a);

	return ret;
}

/*
 * test_memmove_overlapping -- tests memmove vdm operation in case
 * destination and source addresses are overlapping. The size parameter
 * should be a multiply of 4.
 */
int
test_memmove_overlapping(size_t size)
{
	int ret = 0;

	/* assert that size is a multiply of 4 */
	if (size % 4 != 0) {
		fprintf(stderr,
				"size parameter is not a multiply of 4");
		return 1;
	}

	char *buffer = malloc(size + 1);
	if (buffer == NULL) {
		fprintf(stderr,
				"memory for the buffer could not be allocated");
		return 1;
	}

	/* fill up the buffer with 'As', 'Bs' and 'Cs' */
	memset(buffer, 'A', size / 4);
	memset(buffer + size / 4, 'B', size / 4);
	memset(buffer + 2 * size / 4, 'C', size / 4);

	struct data_mover_sync *dms = data_mover_sync_new();
	if (dms == NULL) {
		ret = 1;
		fprintf(stderr,
				"error while creating synchronous data mover");
		goto cleanup_1;
	}

	struct vdm *sync_mover = data_mover_sync_get_vdm(dms);
	if (sync_mover == NULL) {
		ret = 1;
		fprintf(stderr,
				"error while extracting synchronous vdm");
		goto cleanup_2;
	}

	/*
	 * Copy 'As' and 'Bs' from the beginning of the buffer
	 * to the address where 'Bs' start.
	 */
	struct vdm_operation_future test_memmove_fut =
		vdm_memmove(sync_mover, buffer + size / 4,
					buffer, 2 * size / 4, 0);

	FUTURE_BUSY_POLL(&test_memmove_fut);

	for (size_t i = 0; i < 2 * size / 4; i++) {
		if (buffer[i] != 'A')
			ret = 1;
	}

	for (size_t i = 2 * size / 4; i < 3 * size / 4; i++) {
		if (buffer[i] != 'B')
			ret = 1;
	}

	if (ret)
		fprintf(stderr,
				"incorrect values in buffer after memmove");

	cleanup_2:
	data_mover_sync_delete(dms);

	cleanup_1:
	free(buffer);

	return ret;
}

/*
 * test_supported_flags -- test if data_mover_sync support correct flags
 */
int test_supported_flags() {
	struct data_mover_sync *dms = data_mover_sync_new();
	if (dms == NULL) {
		fprintf(stderr,
				"error while creating synchronous data mover");
		return 1;
	}
	struct vdm *sync_mover = data_mover_sync_get_vdm(dms);
	int ret = test_flag(sync_mover, VDM_F_MEM_DURABLE, 0);
	ret += test_flag(sync_mover, VDM_F_NO_CACHE_HINT, 0);
	data_mover_sync_delete(dms);
	return ret;
}

int
main(void)
{
	return
		test_basic_memmove() ||
		test_memmove_overlapping(4) ||
		test_memmove_overlapping(12) ||
		test_memmove_overlapping(1024) ||
		test_supported_flags();
}

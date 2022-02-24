// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "libminiasync.h"
#include "core/os.h"
#include "libminiasync/data_mover_threads.h"
#include "test_helpers.h"

enum test_type {SEQUENCE, SINGLE_CHAR};
/*
 * test_threads_memcpy_multiple -- test multiple memcpy operations n times
 * in a loop on a specific descriptor and test type.
 */
int
test_threads_memcpy_multiple(unsigned memcpy_count,
	unsigned n, size_t test_size, enum test_type test_type)
{
	int ret = 0;
	unsigned seed = (unsigned)time(NULL);
	struct runtime *r = runtime_new();
	struct data_mover_threads *dmt = data_mover_threads_default();

	if (dmt == NULL) {
		runtime_delete(r);
		return 1;
	}

	struct vdm *vdm = data_mover_threads_get_vdm(dmt);

	char **sources = malloc(memcpy_count * sizeof(char *) * n);
	if (sources == NULL)
		UT_FATAL("sources out of memory");
	char **destinations = malloc(memcpy_count * sizeof(char *) * n);
	if (destinations == NULL)
		UT_FATAL("destinations out of memory");
	size_t *sizes = malloc(memcpy_count * sizeof(size_t) * n);
	if (sizes == NULL)
		UT_FATAL("sizes out of memory");

	struct future **futures = malloc(
		memcpy_count * sizeof(struct future *) * n);
	if (futures == NULL)
		UT_FATAL("futures out of memory");
	struct vdm_operation_future *memcpy_futures =
		malloc(memcpy_count * sizeof(struct vdm_operation_future) * n);
	if (memcpy_futures == NULL)
		UT_FATAL("memcpy_futures out of memory");

	unsigned index = 0;
	char value = 0;
	for (unsigned iter = 0; iter < n; iter++) {
		for (unsigned i = index; i < index + memcpy_count; i++) {
			if (test_size) {
				sizes[i] = test_size;
			} else {
				sizes[i] = (size_t)os_rand_r(&seed)
					% (1 << 20) + 1;
			}
			sources[i] = malloc(sizes[i] * sizeof(char));
			destinations[i] = malloc(sizes[i] * sizeof(char));

			switch (test_type) {
				case SEQUENCE:
					value = 0;
					for (unsigned j = 0;
						j < sizes[i]; j++) {
						sources[i][j] = value++;
					}
					break;
				case SINGLE_CHAR:
					memset(sources[i], value++, sizes[i]);
					break;
				default:
					break;
			}

			memcpy_futures[i] = vdm_memcpy(vdm, destinations[i],
				sources[i], sizes[i], 0);
			futures[i] =
				FUTURE_AS_RUNNABLE(&memcpy_futures[i]);
		}

		runtime_wait_multiple(r, futures + iter * memcpy_count,
			memcpy_count);
		index += memcpy_count;
	}

	/* Verification */
	for (unsigned i = 0; i < memcpy_count * n; i++) {
		if (memcmp(sources[i], destinations[i], sizes[i]) != 0) {
			fprintf(stderr,
				"Memcpy nr. %u result is wrong! "
				"Returning\n", i);

			ret = 1;
			goto cleanup;
		}
	}

	/* Cleanup */
cleanup:
	for (unsigned i = 0; i < memcpy_count * n; i++) {
		free(sources[i]);
		free(destinations[i]);
	}
	free(sources);
	free(destinations);
	free(sizes);
	free(futures);
	free(memcpy_futures);

	runtime_delete(r);
	data_mover_threads_delete(dmt);
	return ret;
}

int
main(void)
{
	return
		test_threads_memcpy_multiple(100, 10, 10, SINGLE_CHAR) ||
		test_threads_memcpy_multiple(100, 2, 1 << 10, SINGLE_CHAR) ||
		test_threads_memcpy_multiple(100, 10, 128, SINGLE_CHAR) ||
		test_threads_memcpy_multiple(100, 10, 7, SEQUENCE) ||
		test_threads_memcpy_multiple(100, 1, 1 << 10, SEQUENCE) ||
		test_threads_memcpy_multiple(100, 10, 0, SEQUENCE);
}

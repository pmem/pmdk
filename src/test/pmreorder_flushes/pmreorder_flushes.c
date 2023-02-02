// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019-2020, Intel Corporation */

/*
 * pmreorder_flushes.c -- test for store reordering with flushes
 * in different barriers
 *
 * usage: pmreorder_flushes g|c file
 *
 * g - write data in a specific manner - some flushes
 * of the stores are made in different barriers,
 * c - check data consistency - stores should be applied only
 * after flush - no matter in which barrier the flush will happen
 *
 */

#include "unittest.h"
#include "util.h"
#include "valgrind_internal.h"

#if defined(__PPC64__)
#define STORE_SIZE 128
#else
#define STORE_SIZE 64
#endif

static FILE *fp;

struct stores_fields {
	char A[STORE_SIZE];
	char B[STORE_SIZE];
	char C[STORE_SIZE];
	char D[STORE_SIZE];
	char E[STORE_SIZE];
};

/*
 * write_consistent -- (internal) write data in a specific order
 */
static void
write_consistent(struct stores_fields *sf)
{
	/*
	 * STORE (A)
	 * STORE (B)
	 * STORE (C)
	 *
	 * FLUSH (A, B) (no flush C)
	 * FENCE
	 */
	pmem_memset(&sf->A, -1, sizeof(sf->A), PMEM_F_MEM_NODRAIN);
	pmem_memset(&sf->B, 2, sizeof(sf->B), PMEM_F_MEM_NODRAIN);
	pmem_memset(&sf->C, 3, sizeof(sf->C), PMEM_F_MEM_NOFLUSH);
	pmem_drain();

	/*
	 * STORE (A)
	 * STORE (D)
	 *
	 * FLUSH (D) (no flush A, still no flush C)
	 * FENCE
	 */
	pmem_memset(sf->A, 1, sizeof(sf->A), PMEM_F_MEM_NOFLUSH);
	pmem_memset(sf->D, 4, sizeof(sf->D), PMEM_F_MEM_NODRAIN);
	pmem_drain();

	/*
	 * There are two transitive stores now: A (which does not change
	 * it's value) and C (which is modified).
	 *
	 * STORE (D)
	 * STORE (C)
	 *
	 * FLUSH (D) (still no flush A and C)
	 * FENCE
	 */
	pmem_memset(sf->D, 5, sizeof(sf->D), PMEM_F_MEM_NODRAIN);
	pmem_memset(sf->C, 8, sizeof(sf->C), PMEM_F_MEM_NOFLUSH);
	pmem_drain();

	/*
	 * E is modified just to add additional step to the log.
	 * Values of A and C should still be -1, 2.
	 *
	 * STORE (E)
	 * FLUSH (E)
	 * FENCE
	 */
	pmem_memset(sf->E, 6, sizeof(sf->E), PMEM_F_MEM_NODRAIN);
	pmem_drain();

	/*
	 * FLUSH (A, C)
	 * FENCE
	 */
	pmem_flush(sf->A, sizeof(sf->A));
	pmem_flush(sf->C, sizeof(sf->C));
	pmem_drain();
}

/*
 * check_consistency -- (internal) check if stores are made in proper manner
 */
static int
check_consistency(struct stores_fields *sf)
{
	fprintf(fp, "A=%d B=%d C=%d D=%d E=%d\n",
		sf->A[0], sf->B[0], sf->C[0], sf->D[0], sf->E[0]);
	return 0;
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "pmreorder_flushes");

	util_init();

	if ((argc < 4) || (strchr("gc", argv[1][0]) == NULL) ||
			argv[1][1] != '\0')
		UT_FATAL("usage: %s g|c file log_file", argv[0]);

	int fd = OPEN(argv[2], O_RDWR);
	size_t size;
	/* mmap and register in valgrind pmemcheck */
	void *map = pmem_map_file(argv[2], 0, 0, 0, &size, NULL);
	UT_ASSERTne(map, NULL);

	struct stores_fields *sf = map;

	char opt = argv[1][0];

	/* clear the struct to get a consistent start state for writing */
	if (strchr("g", opt))
		pmem_memset_persist(sf, 0, sizeof(*sf));

	switch (opt) {
		case 'g':
			write_consistent(sf);
			break;
		case 'c':
			fp = os_fopen(argv[3], "a");
			if (fp == NULL)
				UT_FATAL("!fopen");
			int ret;
			ret = check_consistency(sf);
			fclose(fp);
			return ret;
		default:
			UT_FATAL("Unrecognized option %c", opt);
	}

	CLOSE(fd);

	DONE(NULL);
}

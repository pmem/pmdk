// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2022, Intel Corporation */

/*
 * pmem_movnt_align.c -- unit test for functions with non-temporal stores
 *
 * usage: pmem_movnt_align [C|F|B|S]
 *
 * C - pmem_memcpy_persist()
 * B - pmem_memmove_persist() in backward direction
 * F - pmem_memmove_persist() in forward direction
 * S - pmem_memset_persist()
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "libpmem.h"
#include "unittest.h"
#include "movnt_align_common.h"

#define N_BYTES (Ut_pagesize * 2)

static int Heavy;

static void *
pmem_memcpy_persist_wrapper(void *pmemdest, const void *src, size_t len,
		unsigned flags)
{
	(void) flags;
	return pmem_memcpy_persist(pmemdest, src, len);
}

static void *
pmem_memcpy_nodrain_wrapper(void *pmemdest, const void *src, size_t len,
		unsigned flags)
{
	(void) flags;
	return pmem_memcpy_nodrain(pmemdest, src, len);
}

static void *
pmem_memmove_persist_wrapper(void *pmemdest, const void *src, size_t len,
		unsigned flags)
{
	(void) flags;
	return pmem_memmove_persist(pmemdest, src, len);
}

static void *
pmem_memmove_nodrain_wrapper(void *pmemdest, const void *src, size_t len,
		unsigned flags)
{
	(void) flags;
	return pmem_memmove_nodrain(pmemdest, src, len);
}

static void *
pmem_memset_persist_wrapper(void *pmemdest, int c, size_t len, unsigned flags)
{
	(void) flags;
	return pmem_memset_persist(pmemdest, c, len);
}

static void *
pmem_memset_nodrain_wrapper(void *pmemdest, int c, size_t len, unsigned flags)
{
	(void) flags;
	return pmem_memset_nodrain(pmemdest, c, len);
}

static void
check_memmove_variants(size_t doff, size_t soff, size_t len)
{
	check_memmove(doff, soff, len, pmem_memmove_persist_wrapper, 0);
	if (!Heavy)
		return;

	check_memmove(doff, soff, len, pmem_memmove_nodrain_wrapper, 0);

	for (int i = 0; i < ARRAY_SIZE(Flags); ++i)
		check_memmove(doff, soff, len, pmem_memmove, Flags[i]);
}

static void
check_memcpy_variants(size_t doff, size_t soff, size_t len)
{
	check_memcpy(doff, soff, len, pmem_memcpy_persist_wrapper, 0);
	if (!Heavy)
		return;

	check_memcpy(doff, soff, len, pmem_memcpy_nodrain_wrapper, 0);

	for (int i = 0; i < ARRAY_SIZE(Flags); ++i)
		check_memcpy(doff, soff, len, pmem_memcpy, Flags[i]);
}

static void
check_memset_variants(size_t off, size_t len)
{
	check_memset(off, len, pmem_memset_persist_wrapper, 0);
	if (!Heavy)
		return;

	check_memset(off, len, pmem_memset_nodrain_wrapper, 0);

	for (int i = 0; i < ARRAY_SIZE(Flags); ++i)
		check_memset(off, len, pmem_memset, Flags[i]);
}

int
main(int argc, char *argv[])
{
	if (argc != 3)
		UT_FATAL("usage: %s type heavy=[0|1]", argv[0]);

	char type = argv[1][0];
	Heavy = argv[2][0] == '1';
	const char *thr = os_getenv("PMEM_MOVNT_THRESHOLD");
	const char *avx = os_getenv("PMEM_AVX");
	const char *avx512f = os_getenv("PMEM_AVX512F");
	const char *movdir64b = os_getenv("PMEM_MOVDIR64B");

	START(argc, argv, "pmem_movnt_align %c %s %savx %savx512f %smovdir64b",
			type,
			thr ? thr : "default",
			avx ? "" : "!",
			avx512f ? "" : "!",
			movdir64b ? "" : "!");

	size_t page_size = Ut_pagesize;
	size_t s;
	switch (type) {
	case 'C': /* memcpy */
		/* mmap with guard pages */
		Src = MMAP_ANON_ALIGNED(N_BYTES, 0);
		Dst = MMAP_ANON_ALIGNED(N_BYTES, 0);
		if (Src == NULL || Dst == NULL)
			UT_FATAL("!mmap");

		Scratch = MALLOC(N_BYTES);

		/* check memcpy with 0 size */
		check_memcpy_variants(0, 0, 0);

		/* check memcpy with unaligned size */
		for (s = 0; s < CACHELINE_SIZE; s++)
			check_memcpy_variants(0, 0, N_BYTES - s);

		/* check memcpy with unaligned begin */
		for (s = 0; s < CACHELINE_SIZE; s++)
			check_memcpy_variants(s, 0, N_BYTES - s);

		/* check memcpy with unaligned begin and end */
		for (s = 0; s < CACHELINE_SIZE; s++)
			check_memcpy_variants(s, s, N_BYTES - 2 * s);

		MUNMAP_ANON_ALIGNED(Src, N_BYTES);
		MUNMAP_ANON_ALIGNED(Dst, N_BYTES);
		FREE(Scratch);

		break;
	case 'B': /* memmove backward */
		/* mmap with guard pages */
		Src = MMAP_ANON_ALIGNED(2 * N_BYTES - page_size, 0);
		Dst = Src + N_BYTES - page_size;
		if (Src == NULL)
			UT_FATAL("!mmap");

		/* check memmove in backward direction with 0 size */
		check_memmove_variants(0, 0, 0);

		/* check memmove in backward direction with unaligned size */
		for (s = 0; s < CACHELINE_SIZE; s++)
			check_memmove_variants(0, 0, N_BYTES - s);

		/* check memmove in backward direction with unaligned begin */
		for (s = 0; s < CACHELINE_SIZE; s++)
			check_memmove_variants(s, 0, N_BYTES - s);

		/*
		 * check memmove in backward direction with unaligned begin
		 * and end
		 */
		for (s = 0; s < CACHELINE_SIZE; s++)
			check_memmove_variants(s, s, N_BYTES - 2 * s);

		MUNMAP_ANON_ALIGNED(Src, 2 * N_BYTES - page_size);
		break;
	case 'F': /* memmove forward */
		/* mmap with guard pages */
		Dst = MMAP_ANON_ALIGNED(2 * N_BYTES - page_size, 0);
		Src = Dst + N_BYTES - page_size;
		if (Src == NULL)
			UT_FATAL("!mmap");

		/* check memmove in forward direction with 0 size */
		check_memmove_variants(0, 0, 0);

		/* check memmove in forward direction with unaligned size */
		for (s = 0; s < CACHELINE_SIZE; s++)
			check_memmove_variants(0, 0, N_BYTES - s);

		/* check memmove in forward direction with unaligned begin */
		for (s = 0; s < CACHELINE_SIZE; s++)
			check_memmove_variants(s, 0, N_BYTES - s);

		/*
		 * check memmove in forward direction with unaligned begin
		 * and end
		 */
		for (s = 0; s < CACHELINE_SIZE; s++)
			check_memmove_variants(s, s, N_BYTES - 2 * s);

		MUNMAP_ANON_ALIGNED(Dst, 2 * N_BYTES - page_size);

		break;
	case 'S': /* memset */
		/* mmap with guard pages */
		Dst = MMAP_ANON_ALIGNED(N_BYTES, 0);
		if (Dst == NULL)
			UT_FATAL("!mmap");

		Scratch = MALLOC(N_BYTES);

		/* check memset with 0 size */
		check_memset_variants(0, 0);

		/* check memset with unaligned size */
		for (s = 0; s < CACHELINE_SIZE; s++)
			check_memset_variants(0, N_BYTES - s);

		/* check memset with unaligned begin */
		for (s = 0; s < CACHELINE_SIZE; s++)
			check_memset_variants(s, N_BYTES - s);

		/* check memset with unaligned begin and end */
		for (s = 0; s < CACHELINE_SIZE; s++)
			check_memset_variants(s, N_BYTES - 2 * s);

		MUNMAP_ANON_ALIGNED(Dst, N_BYTES);
		FREE(Scratch);

		break;
	default:
		UT_FATAL("!wrong type of test");
		break;
	}

	DONE(NULL);
}

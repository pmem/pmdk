// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2019, Intel Corporation */

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

#define CACHELINE 64
#define N_BYTES (Ut_pagesize * 2)

typedef void *(*mem_fn)(void *, const void *, size_t);

static char *Src;
static char *Dst;
static char *Scratch;
static int Heavy;

static unsigned Flags[] = {
		0,
		PMEM_F_MEM_NODRAIN,
		PMEM_F_MEM_NONTEMPORAL,
		PMEM_F_MEM_TEMPORAL,
		PMEM_F_MEM_NONTEMPORAL | PMEM_F_MEM_TEMPORAL,
		PMEM_F_MEM_NONTEMPORAL | PMEM_F_MEM_NODRAIN,
		PMEM_F_MEM_WC,
		PMEM_F_MEM_WB,
		PMEM_F_MEM_NOFLUSH,
		/* all possible flags */
		PMEM_F_MEM_NODRAIN | PMEM_F_MEM_NOFLUSH |
			PMEM_F_MEM_NONTEMPORAL | PMEM_F_MEM_TEMPORAL |
			PMEM_F_MEM_WC | PMEM_F_MEM_WB,
};

typedef void *pmem_memcpy_fn(void *pmemdest, const void *src, size_t len,
		unsigned flags);
typedef void *pmem_memmove_fn(void *pmemdest, const void *src, size_t len,
		unsigned flags);
typedef void *pmem_memset_fn(void *pmemdest, int c, size_t len, unsigned flags);

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

/*
 * check_memmove -- invoke check function with pmem_memmove_persist
 */
static void
check_memmove(size_t doff, size_t soff, size_t len, pmem_memmove_fn fn,
		unsigned flags)
{
	memset(Dst + doff, 1, len);
	memset(Src + soff, 0, len);

	fn(Dst + doff, Src + soff, len, flags);

	if (memcmp(Dst + doff, Src + soff, len))
		UT_FATAL("memcpy/memmove failed");
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

/*
 * check_memmove -- invoke check function with pmem_memcpy_persist
 */
static void
check_memcpy(size_t doff, size_t soff, size_t len, pmem_memcpy_fn fn,
		unsigned flags)
{
	memset(Dst, 2, N_BYTES);
	memset(Src, 3, N_BYTES);
	memset(Scratch, 2, N_BYTES);

	memset(Dst + doff, 1, len);
	memset(Src + soff, 0, len);
	memcpy(Scratch + doff, Src + soff, len);

	fn(Dst + doff, Src + soff, len, flags);

	if (memcmp(Dst, Scratch, N_BYTES))
		UT_FATAL("memcpy/memmove failed");
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

/*
 * check_memset -- check pmem_memset_no_drain function
 */
static void
check_memset(size_t off, size_t len, pmem_memset_fn fn, unsigned flags)
{
	memset(Scratch, 2, N_BYTES);
	memset(Scratch + off, 1, len);

	memset(Dst, 2, N_BYTES);
	fn(Dst + off, 1, len, flags);

	if (memcmp(Dst, Scratch, N_BYTES))
		UT_FATAL("memset failed");
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

	START(argc, argv, "pmem_movnt_align %c %s %savx %savx512f", type,
			thr ? thr : "default",
			avx ? "" : "!",
			avx512f ? "" : "!");

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
		for (s = 0; s < CACHELINE; s++)
			check_memcpy_variants(0, 0, N_BYTES - s);

		/* check memcpy with unaligned begin */
		for (s = 0; s < CACHELINE; s++)
			check_memcpy_variants(s, 0, N_BYTES - s);

		/* check memcpy with unaligned begin and end */
		for (s = 0; s < CACHELINE; s++)
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
		for (s = 0; s < CACHELINE; s++)
			check_memmove_variants(0, 0, N_BYTES - s);

		/* check memmove in backward direction with unaligned begin */
		for (s = 0; s < CACHELINE; s++)
			check_memmove_variants(s, 0, N_BYTES - s);

		/*
		 * check memmove in backward direction with unaligned begin
		 * and end
		 */
		for (s = 0; s < CACHELINE; s++)
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
		for (s = 0; s < CACHELINE; s++)
			check_memmove_variants(0, 0, N_BYTES - s);

		/* check memmove in forward direction with unaligned begin */
		for (s = 0; s < CACHELINE; s++)
			check_memmove_variants(s, 0, N_BYTES - s);

		/*
		 * check memmove in forward direction with unaligned begin
		 * and end
		 */
		for (s = 0; s < CACHELINE; s++)
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
		for (s = 0; s < CACHELINE; s++)
			check_memset_variants(0, N_BYTES - s);

		/* check memset with unaligned begin */
		for (s = 0; s < CACHELINE; s++)
			check_memset_variants(s, N_BYTES - s);

		/* check memset with unaligned begin and end */
		for (s = 0; s < CACHELINE; s++)
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

/*
 * Copyright 2020, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *
 *     * Neither the name of the copyright holder nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * pmem2_movnt.c -- test for MOVNT threshold
 *
 * usage: pmem2_movnt
 */

#include "unittest.h"
#include "ut_pmem2_utils.h"
#include "ut_pmem2_config.h"

int
main(int argc, char *argv[])
{
	int fd;
	char *dst;
	char *src;
	struct pmem2_config *cfg;
	struct pmem2_map *map;

	if (argc != 2)
		UT_FATAL("usage: %s file", argv[0]);

	const char *thr = os_getenv("PMEM_MOVNT_THRESHOLD");
	const char *avx = os_getenv("PMEM_AVX");
	const char *avx512f = os_getenv("PMEM_AVX512F");

	START(argc, argv, "pmem2_movnt %s %savx %savx512f",
			thr ? thr : "default",
			avx ? "" : "!",
			avx512f ? "" : "!");

	fd = OPEN(argv[1], O_RDWR);

	PMEM2_CONFIG_NEW(&cfg);
	PMEM2_CONFIG_SET_FD(cfg, fd);
	PMEM2_CONFIG_SET_GRANULARITY(cfg, PMEM2_GRANULARITY_PAGE);

	int ret = pmem2_map(cfg, &map);
	UT_PMEM2_EXPECT_RETURN(ret, 0);

	PMEM2_CONFIG_DELETE(&cfg);

	src = MEMALIGN(64, 8192);
	dst = MEMALIGN(64, 8192);

	memset(src, 0x88, 8192);
	memset(dst, 0, 8192);

	pmem2_memset_fn memset_fn = pmem2_get_memset_fn(map);
	pmem2_memcpy_fn memcpy_fn = pmem2_get_memcpy_fn(map);
	pmem2_memmove_fn memmove_fn = pmem2_get_memmove_fn(map);

	for (size_t size = 1; size <= 4096; size *= 2) {
		memset(dst, 0, 4096);
		memcpy_fn(dst, src, size, PMEM2_F_MEM_NODRAIN);
		UT_ASSERTeq(memcmp(src, dst, size), 0);
		UT_ASSERTeq(dst[size], 0);
	}

	for (size_t size = 1; size <= 4096; size *= 2) {
		memset(dst, 0, 4096);
		memmove_fn(dst, src, size, PMEM2_F_MEM_NODRAIN);
		UT_ASSERTeq(memcmp(src, dst, size), 0);
		UT_ASSERTeq(dst[size], 0);
	}

	for (size_t size = 1; size <= 4096; size *= 2) {
		memset(dst, 0, 4096);
		memset_fn(dst, 0x77, size, PMEM2_F_MEM_NODRAIN);
		UT_ASSERTeq(dst[0], 0x77);
		UT_ASSERTeq(dst[size - 1], 0x77);
		UT_ASSERTeq(dst[size], 0);
	}

	ALIGNED_FREE(dst);
	ALIGNED_FREE(src);

	ret = pmem2_unmap(&map);
	UT_ASSERTeq(ret, 0);

	CLOSE(fd);

	DONE(NULL);
}

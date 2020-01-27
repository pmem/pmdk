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
 * pmem2_memcpy.c -- test for doing a memcpy from libpmem2
 *
 * usage: pmem2_memcpy file destoff srcoff length
 *
 */

#include "unittest.h"
#include "file.h"
#include "ut_pmem2_utils.h"
#include "ut_pmem2_config.h"
#include "memcpy_common.h"


/*
 * do_memcpy_variants -- do_memcpy wrapper that tests multiple variants
 * of memcpy functions
 */
static void
do_memcpy_variants(int fd, char *dest, int dest_off, char *src, int src_off,
		    size_t bytes, const char *file_name, union persist p,
		    memcpy_fn fn)
{
	for (int i = 0; i < ARRAY_SIZE(Flags); ++i) {
		do_memcpy(fd, dest, dest_off, src, src_off, bytes, file_name,
				fn, Flags[i], p);
	}
}

/*
 * do_persist -- wrapper for pmem_2 persist_fn
 */
void
do_persist(union persist p, const void *addr, size_t len)
{
	p.persist_fn(addr, len);
}

int
main(int argc, char *argv[])
{
	int fd;
	char *dest;
	char *src;
	char *src_orig;
	size_t mapped_len;
	struct pmem2_config *cfg;
	struct pmem2_map *map;

	if (argc != 5)
		UT_FATAL("usage: %s file srcoff destoff length", argv[0]);

	const char *thr = os_getenv("PMEM_MOVNT_THRESHOLD");
	const char *avx = os_getenv("PMEM_AVX");
	const char *avx512f = os_getenv("PMEM_AVX512F");

	START(argc, argv, "pmem_memcpy %s %s %s %s %savx %savx512f",
			argv[2], argv[3], argv[4], thr ? thr : "default",
			avx ? "" : "!",
			avx512f ? "" : "!");

	fd = OPEN(argv[1], O_RDWR);
	UT_ASSERT(fd != -1);
	int dest_off = atoi(argv[2]);
	int src_off = atoi(argv[3]);
	size_t bytes = strtoul(argv[4], NULL, 0);

	PMEM2_CONFIG_NEW(&cfg);
	PMEM2_CONFIG_SET_FD(cfg, fd);
	PMEM2_CONFIG_SET_GRANULARITY(cfg, PMEM2_GRANULARITY_PAGE);

	int ret = pmem2_map(cfg, &map);
	UT_PMEM2_EXPECT_RETURN(ret, 0);

	PMEM2_CONFIG_DELETE(&cfg);

	/* src > dst */
	mapped_len = pmem2_map_get_size(map);
	dest = pmem2_map_get_address(map);
	if (dest == NULL)
		UT_FATAL("!could not map file: %s", argv[1]);

	src_orig = src = MMAP(dest + mapped_len, mapped_len,
			PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, 0);
	/*
	 * Its very unlikely that src would not be > dest. pmem_map_file
	 * chooses the first unused address >= 1TB, large
	 * enough to hold the give range, and 1GB aligned. If the
	 * addresses did not get swapped to allow src > dst, log error
	 * and allow test to continue.
	 */
	if (src <= dest) {
		swap_mappings(&dest, &src, mapped_len, fd);
		if (src <= dest)
			UT_FATAL("cannot map files in memory order");
	}
	union persist p;
	pmem2_persist_fn persist_fn = pmem2_get_persist_fn(map);
	p.persist_fn = persist_fn;
	memset(dest, 0, (2 * bytes));
	do_persist(p, dest, 2 * bytes);
	memset(src, 0, (2 * bytes));

	pmem2_memcpy_fn memcpy_fn = pmem2_get_memcpy_fn(map);
	do_memcpy_variants(fd, dest, dest_off, src, src_off, bytes,
		argv[1], p, memcpy_fn);

	/* dest > src */
	swap_mappings(&dest, &src, mapped_len, fd);

	if (dest <= src)
		UT_FATAL("cannot map files in memory order");

	do_memcpy_variants(fd, dest, dest_off, src, src_off, bytes,
		argv[1], p, memcpy_fn);

	ret = pmem2_unmap(&map);
	UT_ASSERTeq(ret, 0);

	MUNMAP(src_orig, mapped_len);

	CLOSE(fd);

	DONE(NULL);
}

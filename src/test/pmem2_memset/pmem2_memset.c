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
 * pmem_memset.c -- unit test for doing a memset
 *
 * usage: pmem_memset file offset length
 */

#include "unittest.h"
#include "file.h"
#include "ut_pmem2_utils.h"
#include "ut_pmem2_config.h"
#include "memset_common.h"

/*
 * do_persist -- wrapper for pmem_2 persist_fn
 */
void
do_persist(union persist p, const void *addr, size_t len)
{
	p.persist_fn(addr, len);
}

static void
do_memset_variants(int fd, char *dest, const char *file_name, size_t dest_off,
		size_t bytes, union persist p, memset_fn fn)
{
	for (int i = 0; i < ARRAY_SIZE(Flags); ++i) {
		do_memset(fd, dest, file_name, dest_off, bytes,
				fn, Flags[i], p);
		if (Flags[i] & PMEMOBJ_F_MEM_NOFLUSH)
			do_persist(p, dest, bytes);
	}
}

int
main(int argc, char *argv[])
{
	int fd;
	char *dest;
	struct pmem2_config *cfg;
	struct pmem2_map *map;

	if (argc != 4)
		UT_FATAL("usage: %s file offset length", argv[0]);

	const char *thr = os_getenv("PMEM_MOVNT_THRESHOLD");
	const char *avx = os_getenv("PMEM_AVX");
	const char *avx512f = os_getenv("PMEM_AVX512F");

	START(argc, argv, "pmem2_memset %s %s %s %savx %savx512f",
			argv[2], argv[3],
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

	dest = pmem2_map_get_address(map);
	if (dest == NULL)
		UT_FATAL("!could not map file: %s", argv[1]);

	size_t dest_off = strtoul(argv[2], NULL, 0);
	size_t bytes = strtoul(argv[3], NULL, 0);

	union persist p;
	pmem2_persist_fn persist_fn = pmem2_get_persist_fn(map);
	p.persist_fn = persist_fn;

	pmem2_memset_fn memset_fn = pmem2_get_memset_fn(map);
	do_memset_variants(fd, dest, argv[1], dest_off, bytes, p, memset_fn);

	ret = pmem2_unmap(&map);
	UT_ASSERTeq(ret, 0);

	CLOSE(fd);

	DONE(NULL);
}

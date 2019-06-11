/*
 * Copyright 2019, Intel Corporation
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
 * pmem_map_file_trunc.c -- test for mapping specially crafted files,
 * which used to confuse Windows libc to truncate it by 1 byte
 *
 * See https://github.com/pmem/pmdk/pull/3728 for full description.
 *
 * usage: pmem_map_file_trunc file
 */

#include "unittest.h"
#define EXPECTED_SIZE (4 * 1024)

/*
 * so called "Ctrl-Z" or EOF character
 * https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/fopen-wfopen
 */
#define FILL_CHAR 0x1a

int
main(int argc, char *argv[])
{
	START(argc, argv, "pmem_map_file_trunc");

	if (argc < 2)
		UT_FATAL("not enough args");

	size_t mapped;
	int ispmem;
	char *p;
	os_stat_t st;

	p = pmem_map_file(argv[1], EXPECTED_SIZE, PMEM_FILE_CREATE, 0644,
		&mapped, &ispmem);
	UT_ASSERT(p);
	UT_ASSERTeq(mapped, EXPECTED_SIZE);

	p[EXPECTED_SIZE - 1] = FILL_CHAR;
	pmem_persist(&p[EXPECTED_SIZE - 1], 1);

	pmem_unmap(p, EXPECTED_SIZE);

	STAT(argv[1], &st);
	UT_ASSERTeq(st.st_size, EXPECTED_SIZE);

	p = pmem_map_file(argv[1], 0, 0, 0644, &mapped, &ispmem);
	UT_ASSERT(p);
	UT_ASSERTeq(mapped, EXPECTED_SIZE);
	UT_ASSERTeq(p[EXPECTED_SIZE - 1], FILL_CHAR);

	pmem_unmap(p, EXPECTED_SIZE);

	STAT(argv[1], &st);
	UT_ASSERTeq(st.st_size, EXPECTED_SIZE);

	DONE(NULL);
}

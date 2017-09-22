/*
 * Copyright 2017, Intel Corporation
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
 * util_extent.c -- unit test for extent iterator
 *
 */

#include "unittest.h"
#include "extent.h"

#define TESTFILE_LEN (100 * 1024 * 1024)

static void
test_extent_iter(const char *path)
{
	int fd = OPEN(path, O_RDWR);
	UT_ASSERTne(fd, -1);

	struct extent extent;
	struct extent_iter *iter = extent_new(fd);
	UT_ASSERTne(iter, NULL);

	size_t total_length = 0;

	while (extent_next(iter, &extent) >= 0) {
		total_length += extent.length;
	}

	UT_ASSERTeq(total_length, TESTFILE_LEN);

	extent_delete(iter);

	CLOSE(fd);
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "util_extent");

	if (argc != 2)
		UT_FATAL("usage: %s file", argv[0]);

	test_extent_iter(argv[1]);

	DONE(NULL);
}

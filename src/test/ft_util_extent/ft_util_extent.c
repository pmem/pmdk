/*
 * Copyright 2018, Intel Corporation
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
 * ft_util_extent.c -- unit test uses fault infection in
 * function "os_extents_get"
 */

#include "unittest.h"
#include "alloc.h"
#include "fault_injection.h"
#include "extent.h"

static int
test_size(const char *path, size_t size)
{
	struct extents *exts = MALLOC(sizeof(struct extents));

	UT_ASSERT(os_extents_count(path, exts) >= 0);

	UT_OUT("exts->extents_count: %u", exts->extents_count);

	if (exts->extents_count > 0) {
		exts->extents = MALLOC(exts->extents_count *
							sizeof(struct extent));

	common_inject_fault_at(PMEM_REALLOC, 1, "os_extents_get");
	UT_ASSERTeq(os_extents_get(path, exts), -1);
	UT_ASSERTeq(errno, ENOMEM);
	}

	FREE(exts);

	return 0;
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "ft_util_extent");
	if (common_fault_injection_enabled()) {
		if (argc != 3)
			UT_FATAL("usage: %s file file-size", argv[0]);

		long long isize = atoi(argv[2]);
		UT_ASSERT(isize > 0);
		size_t size = (size_t)isize;

		test_size(argv[1], size);
	}
	DONE(NULL);
}

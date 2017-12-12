/*
 * Copyright 2016-2017, Intel Corporation
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
 * pmem_is_pmem_linux.c -- Linux specific unit test for is_pmem_proc()
 *
 * usage: pmem_is_pmem_linux op addr len [op addr len ...]
 * where op can be: 'a' (add), 'r' (remove), 't' (test)
 */

#include <stdlib.h>

#include "unittest.h"
#include "mmap.h"

int
main(int argc, char *argv[])
{
	START(argc, argv, "pmem_is_pmem_linux");

	if (argc < 3)
		UT_FATAL("usage: %s op addr len [op addr len ...]",
				argv[0]);

	/* insert memory regions to the list */
	int i;
	for (i = 1; i < argc; i += 3) {
		UT_ASSERT(i + 2 < argc);

		errno = 0;
		void *addr = (void *)strtoull(argv[i + 1], NULL, 0);
		UT_ASSERTeq(errno, 0);

		size_t len = strtoull(argv[i + 2], NULL, 0);
		UT_ASSERTeq(errno, 0);

		int ret;

		switch (argv[i][0]) {
		case 'a':
			ret = util_range_register(addr, len);
			UT_ASSERTeq(ret, 0);
			break;
		case 'r':
			ret = util_range_unregister(addr, len);
			UT_ASSERTeq(ret, 0);
			break;
		case 't':
			UT_OUT("addr %p len %zu is_pmem %d",
					addr, len, pmem_is_pmem(addr, len));
			break;
		default:
			FATAL("invalid op");
		}
	}

	DONE(NULL);
}

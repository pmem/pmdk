/*
 * Copyright 2017-2019, Intel Corporation
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
 * libpmempool_rm_win -- a unittest for pmempool_rm.
 *
 */

#include <stddef.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include "unittest.h"

#define FATAL_USAGE(n) UT_FATAL("usage: %s [-f -l -r] path..", (n))

static PMEMobjpool *Pop;

int
wmain(int argc, wchar_t *argv[])
{
	STARTW(argc, argv, "libpmempool_rm_win");
	if (argc < 2)
		FATAL_USAGE(ut_toUTF8(argv[0]));

	unsigned flags = 0;
	int do_open = 0;
	int i = 1;
	for (; i < argc - 1; i++) {
		wchar_t *optarg = argv[i + 1];
		if (wcscmp(L"-f", argv[i]) == 0)
			flags |= PMEMPOOL_RM_FORCE;
		else if (wcscmp(L"-r", argv[i]) == 0)
			flags |= PMEMPOOL_RM_POOLSET_REMOTE;
		else if (wcscmp(L"-l", argv[i]) == 0)
			flags |= PMEMPOOL_RM_POOLSET_LOCAL;
		else if (wcscmp(L"-o", argv[i]) == 0)
			do_open = 1;
		else if (wcschr(argv[i], L'-') == argv[i])
			FATAL_USAGE(argv[0]);
		else
			break;
	}

	for (; i < argc; i++) {
		const wchar_t *path = argv[i];
		if (do_open) {
			Pop = pmemobj_openW(path, NULL);
			UT_ASSERTne(Pop, NULL);
		}
		int ret = pmempool_rmW(path, flags);
		if (ret) {
			UT_OUT("!%s: %s", ut_toUTF8(path),
				pmempool_errormsgU());
		}

		if (do_open) {
			UT_ASSERTne(Pop, NULL);
			pmemobj_close(Pop);
		}
	}

	DONEW(NULL);
}

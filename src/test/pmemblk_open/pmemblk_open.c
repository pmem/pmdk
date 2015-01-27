/*
 * Copyright (c) 2015, Intel Corporation
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
 *     * Neither the name of Intel Corporation nor the names of its
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
 * pmemblk_open.c -- unit test for opening a block memory pool
 *
 * usage: pmemblk_open path bsize_create bsize_open
 */

#include "unittest.h"

#define	SIZEOF_TESTFILE	(64 * 1024 * 1024)
#define	CREATE_MODE		(0664)

int
main(int argc, char *argv[])
{
	START(argc, argv, "pmemblk_open");

	if (argc != 4)
		FATAL("usage: %s path bsize_create bsize_open", argv[0]);

	PMEMblkpool *handle;
	const char *path = argv[1];
	size_t bsize_create = strtoul(argv[2], NULL, 0);
	size_t bsize_open = strtoul(argv[3], NULL, 0);

	if (strcmp(path, "NULLFILE") == 0) {
		if ((handle = pmemblk_create("./testfile", bsize_create,
				SIZEOF_TESTFILE, CREATE_MODE)) == NULL) {
			OUT("!./testfile: pmemblk_create");
			goto err;
		}

		if ((handle = pmemblk_open("./testfile", bsize_open)) == NULL) {
			OUT("!./testfile: pmemblk_open");
			goto err;
		}
	} else {
		if ((handle = pmemblk_create(path, bsize_create,
				0, CREATE_MODE)) == NULL) {
			OUT("!%s: pmemblk_create", path);
			goto err;
		}

		if ((handle = pmemblk_open(path, bsize_open)) == NULL) {
			OUT("!%s: pmemblk_open", path);
			goto err;
		}
	}

err:

	DONE(NULL);
}

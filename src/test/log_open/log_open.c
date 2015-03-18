/*
 * Copyright (c) 2014-2015, Intel Corporation
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
 * log_open.c -- unit test for pmemlog_open()
 *
 * usage: log_open path
 */
#include "unittest.h"

void
pool_check(const char *path)
{
	int result = pmemlog_check(path);

	if (result < 0)
		OUT("!%s: pmemlog_check", path);
	else if (result == 0)
		OUT("%s: pmemlog_check: not consistent", path);
}

void
pool_open(const char *path)
{
	PMEMlogpool *plp = pmemlog_open(path);

	if (plp == NULL)
		OUT("!%s: pmemlog_open", path);
	else {
		OUT("%s: pmemlog_open: Success", path);

		pmemlog_close(plp);
	}
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "log_open");

	if (argc != 2)
		FATAL("usage: %s path", argv[0]);

	const char *path = argv[1];

	PMEMlogpool *plp;

	plp = pmemlog_create(path, (20*1024*1024), 0640);

	if (plp == NULL)
		OUT("!%s: pmemlog_create", path);
	else {
		struct stat stbuf;
		STAT(path, &stbuf);

		OUT("%s: file size %zu mode 0%o",
				path, stbuf.st_size,
				stbuf.st_mode & 0777);

		pmemlog_close(plp);

		pool_check(path);

		pool_open(path);
	}

	if (plp == NULL) {
		pmemlog_create(path, 0, 0640);
		pool_check(path);
		pool_open(path);
	}

	DONE(NULL);
}

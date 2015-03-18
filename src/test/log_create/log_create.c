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
 * log_create.c -- unit test for pmemlog_create()
 *
 * usage: log_create path poolsize mode
 *
 */
#include "unittest.h"

int
main(int argc, char *argv[])
{
	START(argc, argv, "log_create");

	if (argc != 4)
		FATAL("usage: %s path poolsize mode", argv[0]);

	const char *path = argv[1];
	size_t poolsize = strtoul(argv[2], NULL, 0)
		* (1 << 20); /* in megabytes */
	size_t mode = strtoul(argv[3], NULL, 8);

	PMEMlogpool *plp = pmemlog_create(path, poolsize, mode);

	if (plp == NULL)
		OUT("!%s: pmemlog_create", path);
	else {
		struct stat stbuf;
		STAT(path, &stbuf);

		OUT("%s: file size %zu mode 0%o",
				path, stbuf.st_size,
				stbuf.st_mode & 0777);

		pmemlog_close(plp);

		int result = pmemlog_check(path);

		if (result < 0)
			OUT("!%s: pmemlog_check", path);
		else if (result == 0)
			OUT("%s: pmemlog_check: not consistent", path);
	}

	DONE(NULL);
}

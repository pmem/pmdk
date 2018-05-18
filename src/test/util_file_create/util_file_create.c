/*
 * Copyright 2014-2018, Intel Corporation
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
 * util_file_create.c -- unit test for util_file_create()
 *
 * usage: util_file_create minlen len:path [len:path]...
 */

#define _GNU_SOURCE
#include "unittest.h"

#include "file.h"

#ifdef __linux__
#include <errno.h>
#include <fcntl.h>
static void
check_filesystem(const char *file)
{
	int fd = OPEN(file, O_RDWR | O_CREAT | O_EXCL, 0644);

	if (fallocate(fd, 0, 0, 4096)) {
		/*
		 * util_file_create uses posix_fallocate to allocate file space.
		 * posix_fallocate on Linux is implemented using fallocate
		 * syscall. This syscall requires file system-specific code on
		 * the kernel side and not all file systems have this code.
		 * So when posix_fallocate gets 'not supported' error from
		 * fallocate it falls back to just writing zeroes.
		 * This is very slow and can easily make this test fail.
		 * Detect it and refuse to run the test in such case.
		 */
		if (errno == EOPNOTSUPP)
			UT_FATAL(
				"File system backing '%s' does not support fallocate syscall.",
				file);
		UT_FATAL("!fallocate");
	}

	CLOSE(fd);
	UNLINK(file);
}
#else
static void
check_filesystem(const char *file)
{
}
#endif

int
main(int argc, char *argv[])
{
	START(argc, argv, "util_file_create");

	if (argc < 3)
		UT_FATAL("usage: %s minlen len:path...", argv[0]);

	char *fname;
	size_t minsize = strtoul(argv[1], &fname, 0);

	for (int arg = 2; arg < argc; arg++) {
		size_t size = strtoul(argv[arg], &fname, 0);
		if (*fname != ':')
			UT_FATAL("usage: %s minlen len:path...", argv[0]);
		fname++;

		if (size > (1U << 30))
			check_filesystem(fname);

		int fd;
		if ((fd = util_file_create(fname, size, minsize)) == -1)
			UT_OUT("!%s: util_file_create", fname);
		else {
			UT_OUT("%s: created", fname);
			os_close(fd);
		}
	}

	DONE(NULL);
}

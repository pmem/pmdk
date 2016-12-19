/*
 * Copyright 2016, Intel Corporation
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
 * check_if_zeroed -- a tool for checking if part of a regular file or device
 *                    dax is zeroed
 */

#include "common.h"
#include "file.h"
#include "fcntl.h"
#include "mmap.h"

/*
 * print_usage -- print short description of usage
 */
static void
print_usage(void)
{
	printf("usage: check_if_zeroed <file> <length> [offset]\n");
}

int
main(int argc, char *argv[])
{
	int ret = EXIT_SUCCESS;
	char *path = NULL;
	ssize_t len = 0;
	off_t off = 0;

	switch (argc) {
	case 4:
		off = strtol(argv[3], NULL, 0);
	case 3:
		len = strtol(argv[2], NULL, 0);
		path = argv[1];
		break;
	default:
		print_usage();
		exit(EXIT_FAILURE);
	}

	if (off < 0) {
		fprintf(stderr, "offset cannot be negative\n");
		exit(EXIT_FAILURE);
	}

	if (len < 0) {
		fprintf(stderr, "length cannot be negative\n");
		exit(EXIT_FAILURE);
	}

	int fd;
	if ((fd = open(path, O_RDWR)) < 0) {
		fprintf(stderr, "opening %s failed\n", path);
		exit(EXIT_FAILURE);
	}

	ssize_t size = util_file_get_size(path);
	if (size < 0) {
		fprintf(stderr, "getting size of %s failed\n", path);
		ret = EXIT_FAILURE;
		goto out_close;
	}

	void *addr = util_map(fd, (size_t)size, 0, 0);
	if (addr == NULL) {
		fprintf(stderr, "mapping %s (fd = %d) failed, errno %d", path,
				fd, errno);
		ret = EXIT_FAILURE;
		goto out_close;
	}

	ret = util_is_zeroed(ADDR_SUM(addr, off), (size_t)len) ?
			EXIT_SUCCESS : EXIT_FAILURE;

	util_unmap(addr, (size_t)size);

out_close:
	(void) close(fd);
	exit(ret);
}

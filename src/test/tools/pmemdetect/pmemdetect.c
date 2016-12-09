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
 * pmemdetect.c -- detect pmem device
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "mmap.h"
#include "libpmem.h"
#include "file.h"

#define SIZE 4096

/*
 * is_pmem -- checks if given path points to pmem-aware filestem
 */
static int
is_pmem(const char *path)
{
	int ret;

	void *addr = util_map_tmpfile(path, SIZE, 0);
	if (addr == NULL) {
		fprintf(stderr, "file creation failed\n");
		return -1;
	}

	if (pmem_is_pmem(addr, SIZE))
		ret = 1;
	else
		ret = 0;

	util_unmap(addr, SIZE);

	return ret;
}

/*
 * is_dev_Dax -- checks if given path points to device dax
 */
static int
is_dev_dax(const char *path)
{
	if (!util_file_is_device_dax(path)) {
		printf("%s -- not device dax\n", path);
		return 0;
	}

	if (access(path, W_OK|R_OK)) {
		printf("%s -- permission denied\n", path);
		return -1;
	}

	return 1;
}

int
main(int argc, char *argv[])
{
	int ret;

	if (argc != 2 && argc != 3) {
		fprintf(stderr, "usage: %s [-d] path\n", argv[0]);
		exit(2);
	}

	const char *path = argv[1];
	int dev_dax = 0;

	if (argc == 3 && strcmp(argv[1], "-d") == 0) {
		path = argv[2];
		dev_dax = 1;
	}

	util_init();
	util_mmap_init();

	if (dev_dax)
		ret = is_dev_dax(path);
	else
		ret = is_pmem(path);

	/*
	 * Return 0 if 'path' points to PMEM-aware filesystem or device dax.
	 * Otherwise return 1, if any problem occurred return 2.
	 */
	switch (ret) {
	case 0:
		ret = 1;
		break;
	case 1:
		ret = 0;
		break;
	default:
		ret = 2;
		break;
	}

	return ret;
}

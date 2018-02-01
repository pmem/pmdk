/*
 * Copyright 2016-2018, Intel Corporation
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
 * pmemdetect.c -- detect PMEM/Device DAX device or Device DAX alignment
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>

#include "mmap.h"
#include "libpmem.h"
#include "file.h"
#include "os.h"

#define SIZE 4096

#define DEVDAX_DETECT	(1 << 0)
#define DEVDAX_ALIGN	(1 << 1)

/* arguments */
static int Opts;
static char *Path;
static size_t Align;

/*
 * print_usage -- print short description of usage
 */
static void
print_usage(void)
{
	printf("Usage: pmemdetect [options] <path>\n");
	printf("Valid options:\n");
	printf("-d, --devdax    - check if <path> is Device DAX\n");
	printf("-a, --align=N   - check Device DAX alignment\n");
	printf("-h, --help      - print this usage info\n");
}

/*
 * long_options -- command line options
 */
static const struct option long_options[] = {
	{"devdax",	no_argument,		NULL,	'd'},
	{"align",	required_argument,	NULL,	'a'},
	{"help",	no_argument,		NULL,	'h'},
	{NULL,		0,			NULL,	 0 },
};

/*
 * parse_args -- (internal) parse command line arguments
 */
static int
parse_args(int argc, char *argv[])
{
	int opt;
	while ((opt = getopt_long(argc, argv, "a:dh",
			long_options, NULL)) != -1) {
		switch (opt) {
		case 'd':
			Opts |= DEVDAX_DETECT;
			break;
		case 'a':
			Opts |= DEVDAX_ALIGN;
			char *endptr;
			errno = 0;
			size_t align = strtoull(optarg, &endptr, 0);
			if ((endptr && *endptr != '\0') || errno) {
				fprintf(stderr, "'%s' -- invalid alignment",
						optarg);
				return -1;
			}
			Align = (size_t)align;
			break;
		case 'h':
			print_usage();
			exit(EXIT_SUCCESS);
		default:
			print_usage();
			exit(EXIT_FAILURE);
		}
	}

	if (optind < argc) {
		Path = argv[optind];
	} else {
		print_usage();
		exit(EXIT_FAILURE);
	}

	return 0;
}

/*
 * is_pmem -- checks if given path points to pmem-aware filesystem
 */
static int
is_pmem(const char *path)
{
	int ret;
	int flags;
	size_t size;

	os_stat_t buf;
	ret = os_stat(path, &buf);
	if (ret) {
		if (errno != ENOENT) {
			perror(path);
			return -1;
		}

		flags = PMEM_FILE_CREATE;
		size = SIZE;

	} else if (S_ISDIR(buf.st_mode)) {
		flags = PMEM_FILE_CREATE | PMEM_FILE_TMPFILE;
		size = SIZE;
	} else {
		size = 0;
		flags = 0;
	}

	int is_pmem;
	void *addr = pmem_map_file(path, size, flags, 0, &size, &is_pmem);
	if (addr == NULL) {
		perror("pmem_map_file failed");
		return -1;
	}

	util_unmap(addr, size);

	return is_pmem;
}

/*
 * is_dev_dax -- checks if given path points to Device DAX
 */
static int
is_dev_dax(const char *path)
{
	if (os_access(path, W_OK|R_OK)) {
		printf("%s -- permission denied\n", path);
		return -1;
	}

	if (!util_file_is_device_dax(path)) {
		printf("%s -- not device dax\n", path);
		return 0;
	}

	return 1;
}

/*
 * is_dev_dax_align -- checks if Device DAX alignment is as specified
 */
static int
is_dev_dax_align(const char *path, size_t req_align)
{
	if (is_dev_dax(path) != 1)
		return -1;

	size_t align = util_file_device_dax_alignment(path);
	return (req_align == align) ? 1 : 0;
}

int
main(int argc, char *argv[])
{
#ifdef _WIN32
	wchar_t **wargv = CommandLineToArgvW(GetCommandLineW(), &argc);
	for (int i = 0; i < argc; i++) {
		argv[i] = util_toUTF8(wargv[i]);
		if (argv[i] == NULL) {
			for (i--; i >= 0; i--)
				free(argv[i]);
			fprintf(stderr, "Error during arguments conversion\n");
			return 2;
		}
	}
#endif
	int ret;

	if (parse_args(argc, argv)) {
		ret = 2;
		goto out;
	}

	util_init();
	util_mmap_init();

	if (Opts & DEVDAX_DETECT)
		ret = is_dev_dax(Path);
	else if (Opts & DEVDAX_ALIGN)
		ret = is_dev_dax_align(Path, Align);
	else
		ret = is_pmem(Path);

	/*
	 * Return 0 on 'true'. Otherwise return 1.
	 * If any problem occurred return 2.
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

	util_mmap_fini();
out:
#ifdef _WIN32
	for (int i = argc; i > 0; i--)
		free(argv[i - 1]);
#endif
	return ret;
}

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
 * cmpmap -- a tool for comparing files using mmap
 */

#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <sys/mman.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include "file.h"
#include "fcntl.h"
#include "mmap.h"
#include "os.h"
#include "util.h"

#define CMPMAP_ZERO (1<<0)

#define ADDR_SUM(vp, lp) ((void *)((char *)(vp) + (lp)))

/* arguments */
static char *File1 = NULL;	/* file1 name */
static char *File2 = NULL;	/* file2 name */
static size_t Length = 0;	/* number of bytes to read */
static os_off_t Offset = 0;	/* offset from beginning of file */
static int Opts = 0;		/* options flag */

/*
 * print_usage -- print short description of usage
 */
static void
print_usage(void)
{
	printf("Usage: cmpmap [options] file1 [file2]\n");
	printf("Valid options:\n");
	printf("-l, --length=N      - compare up to N bytes\n");
	printf("-o, --offset=N      - skip N bytes at start of the files\n");
	printf("-z, --zero          - compare bytes of the file1 to NUL\n");
	printf("-h, --help          - print this usage info\n");
}

/*
 * long_options -- command line options
 */
static const struct option long_options[] = {
	{"length",	required_argument,	NULL,	'l'},
	{"offset",	required_argument,	NULL,	'o'},
	{"zero",	no_argument,		NULL,	'z'},
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
	char *endptr;
	os_off_t off;
	ssize_t len;
	while ((opt = getopt_long(argc, argv, "l:o:zh",
			long_options, NULL)) != -1) {
		switch (opt) {
		case 'l':
			errno = 0;
			len = strtoll(optarg, &endptr, 0);
			if ((endptr && *endptr != '\0') || errno || len < 0) {
				fprintf(stderr, "'%s' -- invalid length",
						optarg);
				return -1;
			}
			Length = (size_t)len;
			break;
		case 'o':
			errno = 0;
			off = strtol(optarg, &endptr, 0);
			if ((endptr && *endptr != '\0') || errno || off < 0) {
				fprintf(stderr, "'%s' -- invalid offset",
						optarg);
				return -1;
			}
			Offset = off;
			break;
		case 'z':
			Opts |= CMPMAP_ZERO;
			break;
		case 'h':
			print_usage();
			return 0;
		default:
			print_usage();
			return -1;
		}
	}

	if (optind < argc) {
		File1 = argv[optind];
		if (optind + 1 < argc)
			File2 = argv[optind + 1];
	} else {
		print_usage();
		return -1;
	}

	return 0;
}

/*
 * validate_args -- (internal) validate arguments
 */
static int
validate_args(void)
{
	if (File1 == NULL) {
		fprintf(stderr, "no file provided");
		return -1;
	} else if (File2 == NULL && Length == 0) {
		fprintf(stderr, "length of the file has to be provided");
		return -1;
	}
	return 0;
}

/*
 * do_cmpmap -- (internal) perform cmpmap
 */
static int
do_cmpmap(void)
{
	int ret = 0;
	int fd1;
	int fd2;
	size_t size1;
	size_t size2;

	/* open the first file */
	if ((fd1 = os_open(File1, O_RDONLY)) < 0) {
		fprintf(stderr, "opening %s failed, errno %d\n", File1, errno);
		return -1;
	}
	ssize_t size_tmp = util_file_get_size(File1);
	if (size_tmp < 0) {
		fprintf(stderr, "getting size of %s failed, errno %d\n", File1,
				errno);
		ret = -1;
		goto out_close1;
	}
	size1 = (size_t)size_tmp;

	int flag = MAP_SHARED;

	if (Opts & CMPMAP_ZERO) {
		/* when checking if bytes are zeroed */
		fd2 = -1;
		size2 = (size_t)Offset + Length;
		flag |= MAP_ANONYMOUS;
	} else if (File2 != NULL) {
		/* when comparing two files */
		/* open the second file */
		if ((fd2 = os_open(File2, O_RDONLY)) < 0) {
			fprintf(stderr, "opening %s failed, errno %d\n",
					File2, errno);
			ret = -1;
			goto out_close1;
		}
		size_tmp = util_file_get_size(File2);
		if (size_tmp < 0) {
			fprintf(stderr, "getting size of %s failed, errno %d\n",
					File2, errno);
			ret = -1;
			goto out_close2;
		}
		size2 = (size_t)size_tmp;

		/* basic check */
		size_t min_size = (size1 < size2) ? size1 : size2;
		if ((size_t)Offset + Length > min_size) {
			if (size1 != size2) {
				fprintf(stdout, "%s %s differ in size: %zu"
					" %zu\n", File1, File2, size1, size2);
				ret = -1;
				goto out_close2;
			} else {
				Length = min_size - (size_t)Offset;
			}
		}
	} else {
		assert(0);
	}

	/* initialize utils */
	util_init();

	/* map the first file */
	void *addr1;
	if ((addr1 = util_map(fd1, 0, size1, MAP_SHARED,
			1, 0, NULL)) == MAP_FAILED) {
		fprintf(stderr, "mmap failed, file %s, length %zu, offset 0,"
				" errno %d\n", File1, size1, errno);
		ret = -1;
		goto out_close2;
	}

	/* map the second file, or do anonymous mapping to get zeroed bytes */
	void *addr2;
	if ((addr2 = util_map(fd2, 0, size2, flag, 1, 0, NULL)) == MAP_FAILED) {
		fprintf(stderr, "mmap failed, file %s, length %zu, errno %d\n",
			File2 ? File2 : "(anonymous)", size2, errno);
		ret = -1;
		goto out_unmap1;
	}

	/* compare bytes of memory */
	if ((ret = memcmp(ADDR_SUM(addr1, Offset), ADDR_SUM(addr2, Offset),
			Length))) {
		if (Opts & CMPMAP_ZERO)
			fprintf(stderr, "%s is not zeroed\n", File1);
		else
			fprintf(stderr, "%s %s differ\n", File1, File2);
		ret = -1;
	}

	munmap(addr2, size2);

out_unmap1:
	munmap(addr1, size1);

out_close2:
	if (File2 != NULL)
		(void) os_close(fd2);
out_close1:
	(void) os_close(fd1);
	return ret;
}


int
main(int argc, char *argv[])
{
#ifdef _WIN32
	util_suppress_errmsg();
	wchar_t **wargv = CommandLineToArgvW(GetCommandLineW(), &argc);
	for (int i = 0; i < argc; i++) {
		argv[i] = util_toUTF8(wargv[i]);
		if (argv[i] == NULL) {
			for (i--; i >= 0; i--)
				free(argv[i]);
			fprintf(stderr, "Error during arguments conversion\n");
			return 1;
		}
	}
#endif
	int ret = EXIT_FAILURE;

	if (parse_args(argc, argv))
		goto end;

	if (validate_args())
		goto end;

	if (do_cmpmap())
		goto end;

	ret = EXIT_SUCCESS;
end:
#ifdef _WIN32
	for (int i = argc; i > 0; i--)
		free(argv[i - 1]);
#endif
	exit(ret);
}

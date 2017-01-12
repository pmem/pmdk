/*
 * Copyright 2017, Intel Corporation
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

#include <getopt.h>
#include <sys/mman.h>
#include <assert.h>
#include <string.h>
#include "common.h"
#include "file.h"
#include "fcntl.h"
#include "mmap.h"

#define CMPMAP_ZERO (1<<0)

/* arguments */
static char *file1 = NULL;	/* file1 name */
static char *file2 = NULL;	/* file2 name */
static size_t length = 0;	/* number of bytes to read */
static off_t offset = 0;	/* offset from beginning of file */
static int opts = 0;		/* options flag */

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
	{"length",	required_argument,	0,	'l'},
	{"offset",	required_argument,	0,	'o'},
	{"zero",	no_argument,		0,	'z'},
	{"help",	no_argument,		0,	'h'},
	{0,		0,			0,	 0 },
};

/*
 * parse_args -- (internal) parse command line arguments
 */
static int
parse_args(int argc, char *argv[])
{
	int opt;
	char *endptr;
	off_t off;
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
			length = (size_t)len;
			break;
		case 'o':
			errno = 0;
			off = strtol(optarg, &endptr, 0);
			if ((endptr && *endptr != '\0') || errno || off < 0) {
				fprintf(stderr, "'%s' -- invalid offset",
						optarg);
				return -1;
			}
			offset = off;
			break;
		case 'z':
			opts |= CMPMAP_ZERO;
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
		file1 = argv[optind];
		if (optind + 1 < argc)
			file2 = argv[optind + 1];
	} else {
		print_usage();
		exit(EXIT_FAILURE);
	}

	return 0;
}

/*
 * validate_args -- (internal) validate arguments
 */
static int
validate_args()
{
	if (file1 == NULL) {
		fprintf(stderr, "no file provided");
		return -1;
	} else if (file2 == NULL && length == 0) {
		fprintf(stderr, "length of the file has to be provided");
		return -1;
	}
	return 0;
}

/*
 * do_cmpmap -- (internal) perform cmpmap
 */
static int
do_cmpmap()
{
	int ret = EXIT_SUCCESS;
	int fd1;
	int fd2;
	size_t size1;
	size_t size2;

	/* open the first file */
	if ((fd1 = open(file1, O_RDONLY)) < 0) {
		fprintf(stderr, "opening %s failed, errno %d\n", file1, errno);
		exit(EXIT_FAILURE);
	}
	ssize_t size_tmp = util_file_get_size(file1);
	if (size_tmp < 0) {
		fprintf(stderr, "getting size of %s failed, errno %d\n", file1,
				errno);
		ret = EXIT_FAILURE;
		goto out_close1;
	}
	size1 = (size_t)size_tmp;

	int flag = MAP_SHARED;

	if (opts & CMPMAP_ZERO) {
		/* when checking if bytes are zeroed */
		fd2 = -1;
		size2 = length;
		flag |= MAP_ANONYMOUS;
	} else if (file2 != NULL) {
		/* when comparing two files */
		/* open the second file */
		if ((fd2 = open(file2, O_RDONLY)) < 0) {
			fprintf(stderr, "opening %s failed, errno %d\n",
					file2, errno);
			ret = EXIT_FAILURE;
			goto out_close1;
		}
		size_tmp = util_file_get_size(file2);
		if (size_tmp < 0) {
			fprintf(stderr, "getting size of %s failed, errno %d\n",
					file2, errno);
			ret = EXIT_FAILURE;
			goto out_close2;
		}
		size2 = (size_t)size_tmp;

		/* basic check */
		size_t min_size = (size1 < size2) ? size1 : size2;
		if (length > min_size) {
			if (size1 != size2) {
				fprintf(stdout, "%s %s differ in size: %zu"
					" %zu\n", file1, file2, size1, size2);
				ret = EXIT_FAILURE;
				goto out_close2;
			} else {
				length = min_size;
			}
		}
	} else {
		assert(0);
	}

	/* map the first file */
	void *addr1 = NULL;
	if ((addr1 = mmap(NULL, size1, PROT_READ, MAP_SHARED, fd1,
			0)) == MAP_FAILED) {
		fprintf(stderr, "mmap failed, file %s, length %zu, offset 0,"
				" errno %d\n", file1, size1, errno);
		ret = EXIT_FAILURE;
		goto out_close2;
	};

	/* map the second file, or do anonymous mapping to get zeroed bytes */
	void *addr2 = NULL;
	if ((addr2 = mmap(NULL, size2, PROT_READ, flag, fd2, 0)) ==
			MAP_FAILED) {
		fprintf(stderr, "mmap failed, file %s, length %zu, errno %d\n",
			file2 ? file2 : "(anonymous)", size2, errno);
		ret = EXIT_FAILURE;
		goto out_unmap1;
	};

	/* compare bytes of memory */
	if ((ret = memcmp(addr1, addr2, length))) {
		if (opts & CMPMAP_ZERO)
			fprintf(stdout, "%s is not zeroed\n", file1);
		else
			fprintf(stdout, "%s %s differ\n", file1, file2);
		ret = EXIT_FAILURE;
	}

	util_unmap(addr2, size2);

out_unmap1:
	util_unmap(addr1, size1);

out_close2:
	if (file2 != NULL)
		(void) close(fd2);
out_close1:
	(void) close(fd1);
	exit(ret);
}


int
main(int argc, char *argv[])
{
	if (parse_args(argc, argv))
		exit(EXIT_FAILURE);

	if (validate_args())
		exit(EXIT_FAILURE);

	do_cmpmap();
}

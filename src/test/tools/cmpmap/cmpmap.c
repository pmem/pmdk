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
 * cmpmap -- a tool for comparing files using mmap
 */

#include <getopt.h>
#include "common.h"
#include "output.h"
#include "file.h"
#include "fcntl.h"
#include "mmap.h"

#define CMPMAP_ZERO 1<<0

/*
 * cmpmap_context -- context and arguments
 */
struct cmpmap_context {
	char *file1;	/* file1 name */
	char *file2;	/* file2 name */
	size_t len;	/* number of bytes to read */
	off_t off;	/* offset from beginning of file */
	int opts;	/* options flag */
};

/*
 * the default context, with all fields initialized to zero or NULL
 */
static struct cmpmap_context cmpmap_default;

/*
 * print_usage -- print short description of usage
 */
static void
print_usage(void)
{
	printf("usage: check_if_zeroed <file> <length> [offset]\n");
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
parse_args(struct cmpmap_context *ctx, int argc, char *argv[])
{
	int opt;
	char *endptr;
	off_t off;
	ssize_t len;
	while ((opt = getopt_long(argc, argv, "l:o:zh",
			long_options, NULL)) != -1) {
		switch (opt) {
		case 'l':
			len = strtol(optarg, &endptr, 0);
			if ((endptr && *endptr != '\0') || errno || len < 0) {
				outv_err("'%s' -- invalid length", optarg);
				return -1;
			}
			ctx->len = (size_t)len;
			break;
		case 'o':
			off = strtol(optarg, &endptr, 0);
			if ((endptr && *endptr != '\0') || errno || off < 0) {
				outv_err("'%s' -- invalid offset", optarg);
				return -1;
			}
			ctx->off = off;
			break;
		case 'z':
			ctx->opts |= CMPMAP_ZERO;
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
		ctx->file1 = argv[optind];
		if (optind + 2 < argc)
			ctx->file2 = argv[optind + 1];
	} else {
		print_usage();
		exit(EXIT_FAILURE);
	}

	out_set_vlevel(1);
	return 0;
}

/*
 * validate_args -- (internal) validate arguments
 */
static int
validate_args(struct cmpmap_context *ctx)
{
	if (ctx->file1 == NULL) {
		outv_err("no file provided");
		return -1;
	} else if (ctx->file2 == NULL && ctx->len == 0) {
		outv_err("length of the file has to be provided");
		return -1;
	}
	return 0;
}

/*
 * do_cmpmap -- (internal) perform cmpmap
 */
static int
do_cmpmap(struct cmpmap_context *ctx)
{
	int ret = 0;
	int fd1, fd2 = -1;
	ssize_t size1, size2;

	/* open the first file */
	if ((fd1 = open(ctx->file1, O_RDWR)) < 0) {
		fprintf(stderr, "opening %s failed\n", ctx->file1);
		return -1;
	}
	size1 = util_file_get_size(ctx->file1);
	if (size1 < 0) {
		fprintf(stderr, "getting size of %s failed\n", ctx->file1);
		ret = -1;
		goto out_close1;
	}

	/* open the second file if needed */
	if (ctx->file2 != NULL) {
		if ((fd2 = open(ctx->file2, O_RDWR)) < 0) {
			fprintf(stderr, "opening %s failed\n", ctx->file2);
			ret = -1;
			goto out_close1;
		}
		size2 = util_file_get_size(ctx->file2);
		if (size2 < 0) {
			fprintf(stderr, "getting size of %s failed\n",
					ctx->file2);
			ret = -1;
			goto out_close2;
		}
	}

	/* map the first file */
	void *addr1 = util_map(fd1, (size_t)size1, 0, 0);
	if (addr1 == NULL) {
		fprintf(stderr, "mapping %s (fd = %d) failed, errno %d",
				ctx->file1, fd1, errno);
		ret = -1;
		goto out_close2;
	}

	if (ctx->opts & CMPMAP_ZERO) {
		/* check if the memory is zeroed */
		ret = util_is_zeroed(ADDR_SUM(addr1, ctx->off),
				(size_t)ctx->len) ? 0 : -1;
	} else {
		/* XXX: comparing files - to be added */
	}

	util_unmap(addr1, (size_t)size1);

out_close2:
	if (ctx->file2 != NULL)
		(void) close(fd2);
out_close1:
	(void) close(fd1);
	exit(ret);
}


int
main(int argc, char *argv[])
{
	struct cmpmap_context ctx = cmpmap_default;

	if (parse_args(&ctx, argc, argv))
		exit(EXIT_FAILURE);

	if (validate_args(&ctx))
		exit(EXIT_FAILURE);

	if (do_cmpmap(&ctx)) {
		outv_err("failed to perform cmpmap\n");
		if (errno)
			outv_err("errno: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	exit(EXIT_SUCCESS);
}

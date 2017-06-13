/*
 * Copyright 2014-2017, Intel Corporation
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
 * ddmap.c -- simple app for reading and writing data from/to a regular file or
 *	dax device using mmap instead of file io API
 */

#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include "common.h"
#include "output.h"
#include "mmap.h"
#include "file.h"
#include "util.h"
#include "os.h"

/*
 * ddmap_context -- context and arguments
 */
struct ddmap_context {
	char *file_in;	/* input file name */
	char *file_out;	/* output file name */
	char *str;	/* string data to write */
	off_t offset;	/* offset from beginning of file for read/write */
			/* operations */
	size_t len;	/* number of bytes to read */
	int checksum;	/* compute checksum */
};

/*
 * the default context, with all fields initialized to zero or NULL
 */
static struct ddmap_context ddmap_default;

/*
 * print_usage -- print short description of usage
 */
static void
print_usage(void)
{
	printf("Usage: ddmap [option] ...\n");
	printf("Valid options:\n");
	printf("-i FILE           - read from FILE\n");
	printf("-o FILE           - write to FILE\n");
	printf("-d STRING         - STRING to be written\n");
	printf("-s N              - skip N bytes at start of input/output\n");
	printf("-l N              - read or write up to N bytes at a time\n");
	printf("-c                - compute checksum\n");
	printf("-h                - print this usage info\n");
}

/*
 * long_options -- command line options
 */
static const struct option long_options[] = {
	{"input-file",	required_argument,	NULL,	'i'},
	{"output-file",	required_argument,	NULL,	'o'},
	{"string",	required_argument,	NULL,	'd'},
	{"offset",	required_argument,	NULL,	's'},
	{"length",	required_argument,	NULL,	'l'},
	{"checksum",	no_argument,		NULL, 	'c'},
	{"help",	no_argument,		NULL,	'h'},
	{NULL,		0,			NULL,	 0 },
};

/*
 * ddmap_print_bytes -- (internal) print array of bytes to stdout;
 *	printable ASCII characters are printed normally,
 *	NUL character is printed as a little circle (the degree symbol),
 *	non-printable ASCII characters are printed as centered dots
 */
static void
ddmap_print_bytes(const char *data, size_t len)
{
	for (size_t i = 0; i < len; ++i) {
		if (data[i] == '\0')
			/* print the degree symbol for NUL */
			printf("\u00B0");
		else if (data[i] >= ' ' && data[i] <= '~')
			/* print printable ASCII character */
			printf("%c", data[i]);
		else
			/* print centered dot for non-printable character */
			printf("\u00B7");
	}
	printf("\n");
}

/*
 * ddmap_read -- (internal) read a string from the file at the offset and
 *	print it to stdout
 */
static int
ddmap_read(const char *path, off_t offset, size_t len)
{
	char *read_buff = Zalloc(len + 1);
	if (read_buff == NULL) {
		outv_err("Zalloc(%zu) failed\n", len + 1);
		return -1;
	}

	ssize_t read_len = util_file_pread(path, read_buff, len, offset);
	if (read_len < 0) {
		outv_err("pread failed");
		Free(read_buff);
		return -1;
	} else if ((size_t)read_len < len) {
		outv(1, "read less bytes than requested: %zd vs. %zu\n",
				read_len, len);
	}

	ddmap_print_bytes(read_buff, (size_t)read_len);
	Free(read_buff);
	return 0;
}

/*
 * ddmap_zero -- (internal) zero a range of data in the file
 */
static int
ddmap_zero(const char *path, size_t offset, size_t len)
{
	void *addr;
	ssize_t filesize = util_file_get_size(path);
	if (filesize < 0) {
		outv_err("invalid file size");
		return -1;
	}

	if (offset + len > (size_t)filesize)
		len = (size_t)filesize - offset;

	addr = util_file_map_whole(path);
	if (addr == NULL) {
		outv_err("map failed");
		return -1;
	}

	memset((char *)addr + offset, 0, len);
	util_unmap(addr, (size_t)filesize);
	return 0;
}

/*
 * ddmap_write_data -- (internal) write data to a file
 */
static int
ddmap_write_data(const char *path, const char *data, off_t offset, size_t len)
{
	if (util_file_pwrite(path, data, len, offset) < 0) {
			outv_err("pwrite for dax device failed: path %s,"
				" len %zu, offset %zd", path, len, offset);
			return -1;
	}
	return 0;
}

/*
 * ddmap_write_from_file -- (internal) write data from file to dax device or
 *  file
 */
static int
ddmap_write_from_file(const char *path_in, const char *path_out,
	off_t offset, size_t len)
{
	int fd;
	char *src;
	ssize_t file_in_size = util_file_get_size(path_in);

	fd = os_open(path_in, O_RDONLY);
	if (fd > 0) {
		src = mmap(NULL, (size_t)file_in_size, PROT_READ, MAP_PRIVATE,
			fd, 0);
		ddmap_write_data(path_out, src, offset, len);
		util_unmap(src, (size_t)file_in_size);
		os_close(fd);
	} else
		outv_err("invalid input file path");
	return 0;
}

/*
 * ddmap_write -- (internal) write the string to the file
 */
static int
ddmap_write(const char *path, const char *str, off_t offset, size_t len)
{
	/* calculate how many characters from the string are to be written */
	size_t length;
	size_t str_len = (str != NULL) ? strlen(str) + 1 : 0;
	if (len == 0)
		/* i.e. if 'l' option was not used or was set to 0 */
		length = str_len;
	else
		length = min(len, str_len);

	/* write the string */
	if (length > 0) {
		if (ddmap_write_data(path, str, offset, length))
			return -1;
	}

	/* zero the rest of requested range */
	if (length < len) {
		if (ddmap_zero(path, (size_t)offset + length, len - length))
			return -1;
	}
	return 0;
}

/*
 * compute_checksum -- (internal) compute checksum of a portion of input file
 */
static int
ddmap_checksum(const char *path, size_t len, off_t offset)
{
	int sfd = 0;
	char *src;
	uint64_t checksum;
	ssize_t filesize = util_file_get_size(path);

	if (!util_file_is_device_dax(path)) {
		sfd = os_open(path, O_RDONLY);
		if (sfd < 0) {
			outv_err("invalid file path");
			return -1;
		}

		src = mmap(NULL, len,  PROT_READ, MAP_PRIVATE, sfd, 0);
	} else {
		src = util_file_map_whole(path);
	}

	if (offset != 0) {
		if ((size_t)offset < (size_t)filesize)
			src += offset;
		else
			outv_err("offset beyond the file");
	}

	util_checksum(src, len, &checksum, 1);
	util_unmap(src, (size_t)filesize);

	if (sfd > 0)
		os_close(sfd);

	printf("%" PRIu64 "\n", checksum);
	return 0;
}

/*
 * parse_args -- (internal) parse command line arguments
 */
static int
parse_args(struct ddmap_context *ctx, int argc, char *argv[])
{
	int opt;
	char *endptr;
	off_t offset;
	size_t length;
	while ((opt = getopt_long(argc, argv, "i:o:d:s:l:chv",
			long_options, NULL)) != -1) {
		switch (opt) {
		case 'i':
			ctx->file_in = optarg;
			break;
		case 'o':
			ctx->file_out = optarg;
			break;
		case 'd':
			ctx->str = optarg;
			break;
		case 's':
			offset = strtol(optarg, &endptr, 0);
			if ((endptr && *endptr != '\0') || errno) {
				outv_err("'%s' -- invalid offset", optarg);
				return -1;
			}
			ctx->offset = offset;
			break;
		case 'l':
			length = strtoul(optarg, &endptr, 0);
			if ((endptr && *endptr != '\0') || errno) {
				outv_err("'%s' -- invalid length", optarg);
				return -1;
			}
			ctx->len = length;
			break;
		case 'c':
			ctx->checksum = 1;
			break;
		case 'h':
			print_usage();
			exit(EXIT_SUCCESS);
		case 'v':
			out_set_vlevel(1);
			break;
		default:
			print_usage();
			exit(EXIT_FAILURE);
		}
	}

	return 0;
}

/*
 * validate_args -- (internal) validate arguments
 */
static int
validate_args(struct ddmap_context *ctx)
{
	if ((ctx->file_in == NULL) && (ctx->file_out == NULL)) {
		outv_err("either input file, output file or both files must"
			" be provided");
		return -1;
	} else if (ctx->file_out == NULL) {
		if (ctx->len == 0) {
			outv_err("number of bytes to read has to be provided");
			return -1;
		}
	} else if (ctx->file_in == NULL) {
		if (ctx->str == NULL && ctx->len == 0) {
			outv_err("when writing, 'data' or 'length' option has"
					" to be provided");
			return -1;
		}
	}
	return 0;
}

/*
 * do_ddmap -- (internal) perform ddmap
 */
static int
do_ddmap(struct ddmap_context *ctx)
{
	if ((ctx->file_in != NULL) && (ctx->file_out != NULL)) {
		if (ddmap_write_from_file(ctx->file_in, ctx->file_out,
			ctx->offset, ctx->len))
			return -1;
		return 0;
	}

	if ((ctx->checksum == 1) && (ctx->file_in != NULL)) {
		if (ddmap_checksum(ctx->file_in, ctx->len, ctx->offset))
			return -1;
		return 0;
	}

	if (ctx->file_in != NULL) {
		if (ddmap_read(ctx->file_in, ctx->offset, ctx->len))
			return -1;
	} else { /* ctx->file_out != NULL */
		if (ddmap_write(ctx->file_out, ctx->str, ctx->offset, ctx->len))
			return -1;
	}

	return 0;
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
			outv_err("Error during arguments conversion\n");
			return 1;
		}
	}
#endif
	int ret = 0;

	struct ddmap_context ctx = ddmap_default;

	if ((ret = parse_args(&ctx, argc, argv)))
		goto out;

	if ((ret = validate_args(&ctx)))
		goto out;

	if ((ret = do_ddmap(&ctx))) {
		outv_err("failed to perform ddmap\n");
		if (errno)
			outv_err("errno: %s\n", strerror(errno));
		ret = -1;
		goto out;
	}

out:
#ifdef _WIN32
	for (int i = argc; i > 0; i--)
		free(argv[i - 1]);
#endif
	return ret;
}

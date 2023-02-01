// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2014-2023, Intel Corporation */

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
	size_t offset_in;	/* offset from beginning of input file for */
			/* read/write operations expressed in blocks */
	size_t offset_out;	/* offset from beginning of output file for */
			/* read/write operations expressed in blocks */
	size_t bytes;	/* size of blocks to write at the time */
	size_t count;	/* number of blocks to read/write */
	int checksum;	/* compute checksum */
	int runlen;	/* print bytes as runlen/char sequence */
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
	printf("-s N              - skip N blocks at start of input\n");
	printf("-q N              - skip N blocks at start of output\n");
	printf("-b N              - read/write N bytes at a time\n");
	printf("-n N              - copy N input blocks\n");
	printf("-c                - compute checksum\n");
	printf("-r                - print file content as runlen/char pairs\n");
	printf("-h                - print this usage info\n");
}

/*
 * long_options -- command line options
 */
static const struct option long_options[] = {
	{"input-file",	required_argument,	NULL,	'i'},
	{"output-file",	required_argument,	NULL,	'o'},
	{"string",	required_argument,	NULL,	'd'},
	{"offset-in",	required_argument,	NULL,	's'},
	{"offset-out",	required_argument,	NULL,	'q'},
	{"block-size",	required_argument,	NULL,	'b'},
	{"count",	required_argument,	NULL,	'n'},
	{"checksum",	no_argument,		NULL,	'c'},
	{"runlen",	no_argument,		NULL,	'r'},
	{"help",	no_argument,		NULL,	'h'},
	{NULL,		0,			NULL,	 0 },
};

/*
 * ddmap_print_char -- (internal) print single char
 *
 * Printable ASCII characters are printed normally,
 * NUL character is printed as a little circle (the degree symbol),
 * non-printable ASCII characters are printed as centered dots.
 */
static void
ddmap_print_char(char c)
{
	if (c == '\0')
		/* print the degree symbol for NUL */
		printf("\u00B0");
	else if (c >= ' ' && c <= '~')
		/* print printable ASCII character */
		printf("%c", c);
	else
		/* print centered dot for non-printable character */
		printf("\u00B7");
}

/*
 * ddmap_print_runlen -- (internal) print file content as length/char pairs
 *
 * For each sequence of chars of the same value (could be just 1 byte)
 * print length of the sequence and the char value.
 */
static void
ddmap_print_runlen(char *addr, size_t len)
{
	char c = '\0';
	ssize_t cnt = 0;
	for (size_t i = 0; i < len; i++) {
		if (i > 0 && c != addr[i] && cnt != 0) {
			printf("%zd ", cnt);
			ddmap_print_char(c);
			printf("\n");
			cnt = 0;
		}
		c = addr[i];
		cnt++;
	}
	if (cnt) {
		printf("%zd ", cnt);
		ddmap_print_char(c);
		printf("\n");
	}
}

/*
 * ddmap_print_bytes -- (internal) print array of bytes
 */
static void
ddmap_print_bytes(const char *data, size_t len)
{
	for (size_t i = 0; i < len; ++i) {
		ddmap_print_char(data[i]);
	}
	printf("\n");
}

/*
 * ddmap_read -- (internal) read a string from the file at the offset and
 *	print it to stdout
 */
static int
ddmap_read(const char *path, size_t offset_in, size_t bytes, size_t count,
		int runlen)
{
	size_t len = bytes * count;
	os_off_t offset = (os_off_t)(bytes * offset_in);
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

	if (runlen)
		ddmap_print_runlen(read_buff, (size_t)read_len);
	else
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
ddmap_write_data(const char *path, const char *data,
	os_off_t offset, size_t len)
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
	size_t offset_in, size_t offset_out, size_t bytes,
	size_t count)
{
	char *src, *tmp_src;
	os_off_t offset;
	ssize_t file_in_size = util_file_get_size(path_in);
	size_t data_left, len;

	util_init();
	src = util_file_map_whole(path_in);
	src += (os_off_t)(offset_in * bytes);
	offset = (os_off_t)(offset_out * bytes);

	data_left = (size_t)file_in_size;
	tmp_src = src;
	do {
		len = MIN(data_left, bytes);
		ddmap_write_data(path_out, tmp_src, offset, len);
		tmp_src += len;
		data_left -= len;

		if (data_left == 0) {
			data_left = (size_t)file_in_size;
			tmp_src = src;
		}
		offset += (os_off_t)len;
		count--;
	} while (count > 0);

	util_unmap(src, (size_t)file_in_size);
	return 0;
}

/*
 * ddmap_write -- (internal) write the string to the file
 */
static int
ddmap_write(const char *path, const char *str, size_t offset_in, size_t bytes,
	size_t count)
{
	/* calculate how many characters from the string are to be written */
	size_t length;
	size_t str_len = (str != NULL) ? strlen(str) + 1 : 0;
	os_off_t offset = (os_off_t)(bytes * offset_in);
	size_t len = bytes * count;
	if (len == 0)
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
 * ddmap_checksum -- (internal) compute checksum of a slice of an input file
 */
static int
ddmap_checksum(const char *path, size_t bytes, size_t count, size_t offset_in)
{
	char *src;
	uint64_t checksum;
	ssize_t filesize = util_file_get_size(path);
	os_off_t offset = (os_off_t)(bytes * offset_in);
	size_t len = bytes * count;

	if ((size_t)filesize < len + (size_t)offset) {
		outv_err("offset with length exceed file size");
		return -1;
	}

	util_init();
	src = util_file_map_whole(path);

	util_checksum(src + offset, len, &checksum, 1, 0);
	util_unmap(src, (size_t)filesize);

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
	size_t offset;
	size_t count;
	size_t bytes;
	while ((opt = getopt_long(argc, argv, "i:o:d:s:q:b:n:crhv",
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
			if (ctx->count == 0)
				ctx->count = strlen(ctx->str);
			if (ctx->bytes == 0)
				ctx->bytes = 1;
			break;
		case 's':
			errno = 0;
			offset = strtoul(optarg, &endptr, 0);
			if ((endptr && *endptr != '\0') || errno) {
				outv_err("'%s' -- invalid input offset",
					optarg);
				return -1;
			}
			ctx->offset_in = offset;
			break;
		case 'q':
			errno = 0;
			offset = strtoul(optarg, &endptr, 0);
			if ((endptr && *endptr != '\0') || errno) {
				outv_err("'%s' -- invalid output offset",
					optarg);
				return -1;
			}
			ctx->offset_out = offset;
			break;
		case 'b':
			errno = 0;
			bytes = strtoull(optarg, &endptr, 0);
			if ((endptr && *endptr != '\0') || errno) {
				outv_err("'%s' -- invalid block size", optarg);
				return -1;
			}
			ctx->bytes = bytes;
			break;
		case 'n':
			errno = 0;
			count = strtoull(optarg, &endptr, 0);
			if ((endptr && *endptr != '\0') || errno) {
				outv_err("'%s' -- invalid count", optarg);
				return -1;
			}
			ctx->count = count;
			break;
		case 'c':
			ctx->checksum = 1;
			break;
		case 'r':
			ctx->runlen = 1;
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
		outv_err("an input file and/or an output file must be "
			"provided");
		return -1;
	} else if (ctx->file_out == NULL) {
		if (ctx->bytes == 0) {
			outv_err("number of bytes to read has to be provided");
			return -1;
		}
	} else if (ctx->file_in == NULL) {
		/* ddmap_write requirements */
		if (ctx->str == NULL && (ctx->count * ctx->bytes) == 0) {
			outv_err("when writing, 'data' or 'count' and 'bytes' "
				"have to be provided");
			return -1;
		}
	} else {
		/* scenarios other than ddmap_write requirement */
		if ((ctx->bytes * ctx->count) == 0) {
			outv_err("number of bytes and count must be provided");
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
			ctx->offset_in, ctx->offset_out, ctx->bytes,
			ctx->count))
			return -1;
		return 0;
	}

	if ((ctx->checksum == 1) && (ctx->file_in != NULL)) {
		if (ddmap_checksum(ctx->file_in, ctx->bytes, ctx->count,
			ctx->offset_in))
			return -1;
		return 0;
	}

	if (ctx->file_in != NULL) {
		if (ddmap_read(ctx->file_in, ctx->offset_in, ctx->bytes,
			ctx->count, ctx->runlen))
			return -1;
	} else { /* ctx->file_out != NULL */
		if (ddmap_write(ctx->file_out, ctx->str, ctx->offset_in,
			ctx->bytes, ctx->count))
			return -1;
	}

	return 0;
}

int
main(int argc, char *argv[])
{
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
	return ret;
}

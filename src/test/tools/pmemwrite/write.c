/*
 * Copyright 2014-2016, Intel Corporation
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
 * write.c -- simple app for writing data to pool used by pmempool tests
 */
#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include <libgen.h>
#include <string.h>
#include <sys/queue.h>
#include <sys/mman.h>
#include <inttypes.h>
#include <err.h>
#include <fcntl.h>
#include "common.h"
#include "output.h"
#include <libpmemlog.h>
#include <libpmemblk.h>
#include "mmap.h"
#include <wchar.h>
#include "file.h"

enum pmemwrite_mode {
	BLOCK_MODE,
	STRING_MODE
};

/*
 * pmemwrite -- context and arguments
 */
struct pmemwrite
{
	char *fname;	/* file name */
	int nargs;	/* number of arguments */
	char **args;	/* list of arguments */
	size_t offset;	/* offset from BOF for read/write operations */
	size_t len;	/* number of bytes to read */
};


static struct pmemwrite pmemwrite = {
	.fname = NULL,
	.nargs = 0,
	.args = NULL,
	.offset = 0,
	.len = 0,
};

/*
 * print_usage -- print short description of usage
 */
static void
print_usage(char *appname)
{
	printf("Usage: %s [options] <file> <args>...\n", appname);
	printf("Valid options:\n");
	printf("-b                    - block operations mode (default)\n");
	printf("-s                    - string operations mode\n");
	printf("Valid arguments in block operations mode:\n");
	printf("<blockno>:w:<string>  - write <string> to <blockno> block\n");
	printf("<blockno>:z           - set zero flag on <blockno> block\n");
	printf("<blockno>:z           - set error flag on <blockno> block\n");
	printf("Valid arguments in string operations mode:\n");
	printf("<offset>:w:<string>   - write <string> to the file at offset"
			" <offset>\n");
	printf("<offset>:r:<len>      - read a string of length <len> from the"
			" file at the offset <offset>\n");
	printf("<offset>:z:<len>      - zero the part of length <len> at the"
			" offset <offset> of the file\n");

}

/*
 * pmemwrite_log -- write data to pmemlog pool file
 */
static int
pmemwrite_log(struct pmemwrite *pwp)
{
	PMEMlogpool *plp = pmemlog_open(pwp->fname);

	if (!plp) {
		warn("%s", pwp->fname);
		return -1;
	}

	int i;
	int ret = 0;
	for (i = 0; i < pwp->nargs; i++) {
		size_t len = strlen(pwp->args[i]);
		if (pmemlog_append(plp, pwp->args[i], len)) {
			warn("%s", pwp->fname);
			ret = -1;
			break;
		}
	}

	pmemlog_close(plp);

	return ret;
}

/*
 * pmemwrite_blk -- write data to pmemblk pool file
 */
static int
pmemwrite_blk(struct pmemwrite *pwp)
{
	PMEMblkpool *pbp = pmemblk_open(pwp->fname, 0);

	if (!pbp) {
		warn("%s", pwp->fname);
		return -1;
	}

	int i;
	int ret = 0;
	size_t blksize = pmemblk_bsize(pbp);
	char *blk = malloc(blksize);
	if (!blk) {
		ret = -1;
		outv_err("malloc(%lu) failed\n", blksize);
		goto nomem;
	}

	for (i = 0; i < pwp->nargs; i++) {
		int64_t blockno;
		char *buff;
		size_t buffsize = strlen(pwp->args[i]) + 1;
		buff = malloc(buffsize);
		if (buff == NULL) {
			ret = -1;
			outv_err("malloc(%lu) failed\n", buffsize);
			goto end;
		}
		char flag;
		/* <blockno>:w:<string> - write string to <blockno> */
		if (sscanf(pwp->args[i], "%" SCNi64 ":w:%[^:]",
					&blockno, buff) == 2) {
			memset(blk, 0, blksize);
			size_t bufflen = strlen(buff);
			if (bufflen > blksize) {
				outv_err("String is longer than block size. "
					"Truncating.\n");
				bufflen = blksize;
			}
			memcpy(blk, buff, bufflen);
			ret = pmemblk_write(pbp, blk, blockno);
			free(buff);
			if (ret)
				goto end;
		/* <blockno>:<flag> - set <flag> flag on <blockno> */
		} else if (sscanf(pwp->args[i], "%" SCNi64 ":%c",
					&blockno, &flag) == 2) {
			free(buff);
			switch (flag) {
			case 'z':
				ret = pmemblk_set_zero(pbp, blockno);
				break;
			case 'e':
				ret = pmemblk_set_error(pbp, blockno);
				break;
			default:
				outv_err("Invalid flag '%c'\n", flag);
				ret = -1;
				goto end;
			}
			if (ret) {
				warn("%s", pwp->fname);
				goto end;
			}
		} else {
			free(buff);
			outv_err("Invalid argument '%s'\n", pwp->args[i]);
			ret = -1;
			goto end;
		}
	}
end:
	free(blk);

nomem:
	pmemblk_close(pbp);

	return ret;
}

/*
 * block_operations -- (internal) operations in block mode
 */
static int
block_operations()
{
	struct pmem_pool_params params;

	/* parse pool type from file */

	pmem_pool_parse_params(pmemwrite.fname, &params, 1);

	switch (params.type) {
	case PMEM_POOL_TYPE_BLK:
		return pmemwrite_blk(&pmemwrite);
	case PMEM_POOL_TYPE_LOG:
		return pmemwrite_log(&pmemwrite);
	default:
		break;
	}

	return -1;
}

/*
 * string_write -- (internal) write the string to the file at the offset
 */
static int
string_write(const char *path, off_t offset, const char *buff)
{
	if (offset < 0) {
		outv_err("invalid argument(s)");
		return -1;
	}
	size_t len = strlen(buff);
	if (util_file_pwrite(path, buff, len, offset) < 0) {
		outv_err("pwrite for dax device failed: path %s,"
			" len %lu, offset %ll", path, len, offset);
		return -1;
	}

	return 0;
}

/*
 * string_read -- (internal) read a string from the file at the offset and
 *	print it to stdout
 */
static int
string_print(const char *path, off_t offset, ssize_t len)
{
	if (offset < 0 || len < 0) {
		outv_err("invalid argument(s)");
		return -1;
	}

	char *read_buff = Zalloc((size_t)len + 1);
	if (read_buff == NULL) {
		outv_err("Zalloc(%lu) failed\n", len + 1);
		return -1;
	}

	ssize_t read_len;
	read_len = util_file_pread(path, read_buff, (size_t)len, offset);

	if (read_len < 0) {
		outv_err("pread failed");
		free(read_buff);
		return -1;
	} else if (read_len < len) {
		outv(1, "read less bytes than requested: %ld vs. %ld\n",
				read_len, len);
	}

	for (int i = 0; i < read_len; ++i) {
		if (read_buff[i] == '\0')
			printf("\u00B0");
		else if (read_buff[i] >= ' ' &&
				read_buff[i] <= '~')
			printf("%c", read_buff[i]);
		else
			printf("\u00B7");
	}
	printf("\n");
	free(read_buff);
	return 0;
}

/*
 * string_zero -- (internal) zero file data at the offset
 */
static int
string_zero(const char *path, off_t offset, ssize_t len)
{
	if (offset < 0 || len < 0) {
		outv_err("invalid argument(s)");
		return -1;
	}

	int is_dax = util_file_is_device_dax(path);
	void *addr;
	ssize_t filesize;
	if (is_dax) {
		filesize = util_file_get_size(path);
		if (filesize < 0) {
			outv_err("invalid file size");
			return -1;
		}
		if (offset + len > filesize)
			len = filesize - offset;

		addr = util_file_map_whole(path);
		if (addr == NULL) {
			outv_err("map failed");
			return -1;
		}
	} else {
		int fd;
		if ((fd = open(path, O_RDWR)) < 0) {
			outv_err("!open %s", path);
			return -1;
		}

		util_stat_t stbuf;
		if (util_fstat(fd, &stbuf) < 0) {
			outv_err("!fstat %s", path);
			(void) close(fd);
			return -1;
		}
		filesize = stbuf.st_size;
		if (filesize < 0) {
			outv_err("invalid file size");
			(void) close(fd);
			return -1;
		}
		if (offset + len > filesize)
			len = filesize - offset;

		addr = mmap(NULL, (size_t)len, PROT_READ|PROT_WRITE, MAP_SHARED,
				fd, (off_t)offset);
		if (addr == MAP_FAILED) {
			ERR("!mmap: %s", path);
			(void) close(fd);
			return -1;
		}
		(void) close(fd);
	}
	memset(ADDR_SUM(addr, offset), 0, (size_t)len);
	util_unmap(addr, (size_t)filesize);
	return 0;
}


/*
 * string_operations -- (internal) operations in string mode
 */
static int
string_operations()
{
	for (int i = 0; i < pmemwrite.nargs; i++) {
		int ret = 0;

		/* params to be get from arguments */
		off_t offset;
		ssize_t len;
		char *write_buff;
		size_t write_buff_size = strlen(pmemwrite.args[i]) + 1;
		write_buff = Zalloc(write_buff_size);
		if (write_buff == NULL) {
			outv_err("Zalloc(%lu) failed\n", write_buff_size);
			return -1;
		}

		/* <offset>:w:<string> - write the string at the offset */
		if (sscanf(pmemwrite.args[i], "%" SCNi64 ":w:%[^:]",
					&offset, write_buff) == 2) {
			ret = string_write(pmemwrite.fname, offset, write_buff);

		/* <offset>:r:<len> - read a string from the offset and print */
		} else if (sscanf(pmemwrite.args[i], "%" SCNi64 ":r:%" SCNi64,
					&offset, &len) == 2) {
			ret = string_print(pmemwrite.fname, offset, len);

		/* <offset>:z:<len> - zero data at the offset */
		} else if (sscanf(pmemwrite.args[i], "%" SCNi64 ":z:%" SCNi64,
					&offset, &len) == 2) {
			ret = string_zero(pmemwrite.fname, offset, len);

		} else {
			outv_err("Invalid argument '%s'\n", pmemwrite.args[i]);
			ret = -1;
		}
		free(write_buff);
		if (ret)
			return ret;
	}
	return 0;
}

int
main(int argc, char *argv[])
{
	int opt;
	util_init();
	char *appname = basename(argv[0]);
	enum pmemwrite_mode mode = BLOCK_MODE;

	while ((opt = getopt(argc, argv, "hbs")) != -1) {
		switch (opt) {
		case 'b':
			mode = BLOCK_MODE;
			break;
		case 's':
			mode = STRING_MODE;
			break;
		case 'h':
			print_usage(appname);
			exit(EXIT_SUCCESS);
		default:
			print_usage(appname);
			exit(EXIT_FAILURE);
		}
	}

	if (optind + 1 < argc) {
		pmemwrite.fname = argv[optind];
		optind++;
		pmemwrite.nargs = argc - optind;
		pmemwrite.args = &argv[optind];
	} else {
		print_usage(appname);
		exit(EXIT_FAILURE);
	}

	out_set_vlevel(1);

	switch (mode) {
	case BLOCK_MODE:
		return block_operations();
	case STRING_MODE:
		return string_operations();
	default:
		break;
	}

	return -1;
}

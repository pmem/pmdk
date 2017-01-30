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
 * bttcreate.c -- tool for generating BTT layout
 */
#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#include "set.h"
#include "pool_hdr.h"
#include "btt.h"
#include "btt_layout.h"
#include "pmemcommon.h"

#define BTT_CREATE_DEF_SIZE	(20 * 1UL << 20) /* 20 MB */
#define BTT_CREATE_DEF_BLK_SIZE	512UL
#define BTT_CREATE_DEF_OFFSET_SIZE	(4 * 1UL << 10) /* 4 KB */

struct btt_context {
	void *addr;
	uint64_t len;
};

struct bbtcreate_options {
	const char *fpath;
	size_t poolsize;
	uint32_t blocksize;
	unsigned maxlanes;
	uuid_t uuid;
	bool trunc;
	bool verbose;
	bool user_uuid;
};

/*
 * nsread -- btt callback for reading
 */
static int
nsread(void *ns, unsigned lane, void *buf, size_t count,
		uint64_t off)
{
	struct btt_context *nsc = (struct btt_context *)ns;

	if (off + count > nsc->len) {
		errno = EINVAL;
		return -1;
	}
	memcpy(buf, (char *)nsc->addr + off, count);
	return 0;
}

/*
 * nswrite -- btt callback for writing
 */
static int
nswrite(void *ns, unsigned lane, const void *buf,
		size_t count, uint64_t off)
{
	struct btt_context *nsc = (struct btt_context *)ns;

	if (off + count > nsc->len) {
		errno = EINVAL;
		return -1;
	}
	memcpy((char *)nsc->addr + off, buf, count);
	return 0;
}

/*
 * nsmap -- btt callback for memory mapping
 */
static ssize_t
nsmap(void *ns, unsigned lane, void **addrp, size_t len,
		uint64_t off)
{
	struct btt_context *nsc = (struct btt_context *)ns;

	assert((ssize_t)len >= 0);
	if (off + len >= nsc->len) {
		errno = EINVAL;
		return -1;
	}

	/*
	 * Since the entire file is memory-mapped, this callback
	 * can always provide the entire length requested.
	 */
	*addrp = (char *)nsc->addr + off;
	return (ssize_t)len;
}

/*
 * nssync -- btt callback for memory synchronization
 */
static void
nssync(void *ns, unsigned lane, void *addr, size_t len)
{
	/* do nothing */
}

/*
 * nszero -- btt callback for zeroing memory
 */
static int
nszero(void *ns, unsigned lane, size_t len, uint64_t off)
{
	struct btt_context *nsc = (struct btt_context *)ns;
	if (off + len >= nsc->len) {
		errno = EINVAL;
		return -1;
	}
	memset((char *)nsc->addr + off, 0, len);
	return 0;
}

/*
 * print_usage -- print usage of program
 */
static void
print_usage(char *name)
{
	printf("Usage: %s [-s <pool_size>] [-b <block_size>] "
		"[-l <max_lanes>] [-u <uuid>] [-t] [-v] "
		"<pool_name>\n", name);
}

/*
 * file_error -- handle file errors
 */
static int
file_error(const int fd, const char *fpath)
{
	if (fd != -1)
		(void) close(fd);
	unlink(fpath);
	return -1;
}

/*
 * print_uuid -- print uuid
 */
static void
print_uuid(uint8_t *uuid)
{
	char uuidstr[POOL_HDR_UUID_STR_LEN];
	if (util_uuid_to_string(uuid, uuidstr) == 0) {
		printf("uuid\t\t%s\n", uuidstr);
	}
}

/*
 * print_result -- print result if verbose option is on
 */
static void
print_result(struct bbtcreate_options *opts)
{
	if (opts->verbose) {
		printf("BTT successfully created: %s\n", opts->fpath);
		printf("poolsize\t%zuB\n", opts->poolsize);
		printf("blocksize\t%uB\n", opts->blocksize);
		printf("maxlanes\t%u\n", opts->maxlanes);
		print_uuid(opts->uuid);
		putchar('\n');
	}
}

int
main(int argc, char *argv[])
{
	common_init("", "", "", 0, 0);

	int opt;
	size_t size;
	int fd;
	int res = 0;
	struct bbtcreate_options opts = {
		.poolsize = BTT_CREATE_DEF_SIZE,
		.blocksize = BTT_CREATE_DEF_BLK_SIZE,
		.maxlanes = BTT_DEFAULT_NFREE,
		.trunc = false,
		.verbose = false,
		.user_uuid = false
	};

	/* parse option */
	while ((opt = getopt(argc, argv, "s:b:l:u:tv")) != -1) {
		switch (opt) {
		case 's':
			if (util_parse_size(optarg, &size) == 0) {
				opts.poolsize = size;
			} else {
				fprintf(stderr, "Wrong size format in pool"
					" size option\n");
				return -1;
			}
			break;
		case 'b':
			if (util_parse_size(optarg, &size) == 0) {
				opts.blocksize = (uint32_t)size;
			} else {
				fprintf(stderr, "Wrong size format in block"
					" size option\n");
				return -1;
			}
			break;
		case 'l':
			opts.maxlanes = (unsigned)strtoul(optarg, NULL, 0);
			break;
		case 'u':
			if (util_uuid_from_string(optarg,
				(struct uuid *)&opts.uuid) == 0) {
				opts.user_uuid = true;
			} else {
				fprintf(stderr, "Wrong uuid format.");
				return -1;
			}
			break;
		case 't':
			opts.trunc = true;
			break;
		case 'v':
			opts.verbose = true;
			break;
		default:
			print_usage(argv[0]);
			return -1;
		}
	}
	if (optind < argc) {
		opts.fpath = argv[optind];
	} else {
		print_usage(argv[0]);
		return -1;
	}

	/* check sizes */
	if (opts.poolsize < BTT_MIN_SIZE) {
		fprintf(stderr, "Pool size is less then %d MB\n",
				BTT_MIN_SIZE >> 20);
		return -1;
	}
	if (opts.blocksize < BTT_MIN_LBA_SIZE) {
		fprintf(stderr, "Block size is less then %zu B\n",
				BTT_MIN_LBA_SIZE);
		return -1;
	}

	/* open file */
	if ((fd = open(opts.fpath, O_RDWR|O_CREAT,
			S_IRUSR|S_IWUSR)) < 0) {
		perror(opts.fpath);
		return -1;
	}

	/* allocate file */
	if (!opts.trunc) {
		if (posix_fallocate(fd, 0,
				(off_t)opts.poolsize) != 0) {
			perror("posix_fallocate");
			res = file_error(fd, opts.fpath);
			goto error;
		}
	} else {
		if (ftruncate(fd, (off_t)opts.poolsize) != 0) {
			perror("ftruncate");
			res = file_error(fd, opts.fpath);
			goto error;
		}
	}

	/* map created file */
	void *base = util_map(fd, opts.poolsize, MAP_SHARED, 0, 0);
	if (!base) {
		perror("util_map");
		res = file_error(fd, opts.fpath);
		goto error_map;
	}

	/* setup btt context */
	struct btt_context btt_context = {
		.addr = (void *)((uint64_t)base + BTT_CREATE_DEF_OFFSET_SIZE),
		.len = opts.poolsize - BTT_CREATE_DEF_OFFSET_SIZE
	};

	/* generate uuid */
	if (!opts.user_uuid) {
		if (util_uuid_generate(opts.uuid) < 0) {
			perror("util_uuid_generate");
			res = -1;
			goto error_map;
		}
	}

	/* init callback structure */
	static struct ns_callback btt_ns_callback = {
		.nsread = nsread,
		.nswrite = nswrite,
		.nsmap = nsmap,
		.nssync = nssync,
		.nszero = nszero,
	};

	/* init btt in requested area */
	struct btt *bttp = btt_init(opts.poolsize - BTT_CREATE_DEF_OFFSET_SIZE,
		opts.blocksize, opts.uuid, opts.maxlanes,
		(void *)&btt_context,
		&btt_ns_callback);
	if (!bttp) {
		printf("Error: Cannot initialize BTT layer\n");
		res = -1;
		goto error_map;
	}

	/* initialize metadata */
	if (btt_set_error(bttp, 0, 0)) {
		perror("btt_set_error");
		res = -1;
		goto error_btt;
	}
	if (btt_set_zero(bttp, 0, 0)) {
		perror("btt_set_zero");
		res = -1;
		goto error_btt;
	}

	/* print results */
	print_result(&opts);


error_btt:
	btt_fini(bttp);
error_map:
	common_fini();
error:
	close(fd);

	return res;
}

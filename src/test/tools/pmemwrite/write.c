/*
 * Copyright 2014-2019, Intel Corporation
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
#include <inttypes.h>
#include <err.h>

#include "common.h"
#include "output.h"
#include <libpmemlog.h>
#include <libpmemblk.h>
#include "mmap.h"
#include "queue.h"

/*
 * pmemwrite -- context and arguments
 */
struct pmemwrite
{
	char *fname;	/* file name */
	int nargs;	/* number of arguments */
	char **args;	/* list of arguments */
};

static struct pmemwrite pmemwrite = {
	.fname = NULL,
	.nargs = 0,
	.args = NULL,
};

/*
 * print_usage -- print short description of usage
 */
static void
print_usage(char *appname)
{
	printf("Usage: %s <file> <args>...\n", appname);
	printf("Valid arguments:\n");
	printf("<blockno>:w:<string>  - write <string> to <blockno> block\n");
	printf("<blockno>:z           - set zero flag on <blockno> block\n");
	printf("<blockno>:z           - set error flag on <blockno> block\n");
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
			if (bufflen == 0) {
				free(buff);
				goto end;
			}
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
			outv_err("Error during arguments conversion\n");
			return 1;
		}
	}
#endif
	int opt;
	int ret = 0;
	util_init();
	char *appname = basename(argv[0]);

	while ((opt = getopt(argc, argv, "h")) != -1) {
		switch (opt) {
		case 'h':
			print_usage(appname);
			ret = 0;
			goto end;
		default:
			print_usage(appname);
			ret = 1;
			goto end;
		}
	}

	if (optind + 1 < argc) {
		pmemwrite.fname = argv[optind];
		optind++;
		pmemwrite.nargs = argc - optind;
		pmemwrite.args = &argv[optind];
	} else {
		print_usage(appname);
		ret = 1;
		goto end;
	}

	out_set_vlevel(1);

	struct pmem_pool_params params;

	/* parse pool type from file */

	pmem_pool_parse_params(pmemwrite.fname, &params, 1);

	switch (params.type) {
	case PMEM_POOL_TYPE_BLK:
		ret = pmemwrite_blk(&pmemwrite);
		break;
	case PMEM_POOL_TYPE_LOG:
		ret = pmemwrite_log(&pmemwrite);
		break;
	default:
		ret = 1;
	}
end:
#ifdef _WIN32
	for (int i = argc; i > 0; i--)
		free(argv[i - 1]);
#endif
	return ret;
}

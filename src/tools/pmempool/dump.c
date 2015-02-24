/*
 * Copyright (c) 2014-2015, Intel Corporation
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
 *     * Neither the name of Intel Corporation nor the names of its
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
 * create.c -- pmempool create command source file
 */

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <err.h>
#include "common.h"
#include "output.h"
#include "libpmemblk.h"
#include "libpmemlog.h"

/*
 * pmempool_dump -- context and arguments for dump command
 */
struct pmempool_dump {
	char *fname;
	int fd;
	char *ofname;
	FILE *ofh;
	int hex;
	uint64_t bsize;
	struct ranges ranges;
	char *str_ranges;
	size_t chunksize;
	uint64_t chunkcnt;
};

/*
 * pmempool_dump_default -- default arguments and context values
 */
static const struct pmempool_dump pmempool_dump_default = {
	.fname		= NULL,
	.fd		= -1,
	.ofname		= NULL,
	.ofh		= NULL,
	.hex		= 1,
	.bsize		= 0,
	.str_ranges	= NULL,
	.chunksize	= 0,
	.chunkcnt	= 0,
};

/*
 * long_options -- command line options
 */
static const struct option long_options[] = {
	{"output",	required_argument,	0,	'o'},
	{"binary",	no_argument,		0,	'b'},
	{"range",	required_argument,	0,	'r'},
	{"chunk",	required_argument,	0,	'c'},
	{"help",	no_argument,		0,	'?'},
	{0,		0,			0,	 0 },
};

/*
 * help_str -- string for help message
 */
static const char *help_str =
"Dump user data from pool\n"
"\n"
"Available options:\n"
"  -o, --output <file>  output file name\n"
"  -b, --binary         dump data in binary format\n"
"  -r, --range <range>  range of bytes/blocks/data chunks\n"
"  -c, --chunk <size>   size of chunk for PMEMLOG pool\n"
"  -?, --help           display this help and exit\n"
"\n"
"For complete documentation see %s-dump(1) manual page.\n"
;

/*
 * print_usage -- print application usage short description
 */
static void
print_usage(char *appname)
{
	printf("Usage: %s dump [<args>] <file>\n", appname);
}

/*
 * print_version -- print version string
 */
static void
print_version(char *appname)
{
	printf("%s %s\n", appname, SRCVERSION);
}

/*
 * pmempool_dump_help -- print help message for dump command
 */
void
pmempool_dump_help(char *appname)
{
	print_usage(appname);
	print_version(appname);
	printf(help_str, appname);
}

/*
 * pmempool_dump_log_process_chunk -- callback for pmemlog_walk
 */
static int
pmempool_dump_log_process_chunk(const void *buf, size_t len, void *arg)
{
	struct pmempool_dump *pdp = (struct pmempool_dump *)arg;

	if (len == 0)
		return 0;

	struct range *curp = NULL;
	if (pdp->chunksize) {
		LIST_FOREACH(curp, &pdp->ranges.head, next) {
			if (pdp->chunkcnt >= curp->first &&
			    pdp->chunkcnt <= curp->last) {
				if (pdp->hex) {
					outv_hexdump(0, buf, pdp->chunksize,
						pdp->chunksize * pdp->chunkcnt,
						0);
				} else {
					if (fwrite(buf, pdp->chunksize,
							1, pdp->ofh) != 1)
						err(1, "%s", pdp->ofname);
				}
			}
		}
		pdp->chunkcnt++;
	} else {
		LIST_FOREACH(curp, &pdp->ranges.head, next) {
			if (curp->first >= len)
				continue;
			uint8_t *ptr = (uint8_t *)buf + curp->first;
			if (curp->last >= len)
				curp->last = len-1;
			uint64_t count = curp->last - curp->first + 1;
			if (pdp->hex) {
				outv_hexdump(0, ptr, count, curp->first, 0);
			} else {
				if (fwrite(ptr, count, 1, pdp->ofh) != 1)
					err(1, "%s", pdp->ofname);
			}
		}
	}

	return 1;
}

/*
 * pmempool_dump_log -- dump data from pmem log pool
 */
static int
pmempool_dump_log(struct pmempool_dump *pdp)
{
	PMEMlogpool *plp = pmemlog_open(pdp->fname);

	if (!plp) {
		warn("%s", pdp->fname);
		return -1;
	}

	struct range entire;
	entire.first = 0;
	entire.last = pmemlog_nbyte(plp)-1;

	if (pdp->str_ranges) {
		if (util_parse_ranges(pdp->str_ranges, &pdp->ranges, &entire)
				!= 0) {
			out_err("Cannot parse range\n");
			pmemlog_close(plp);
			return -1;
		}
	} else {
		util_ranges_add(&pdp->ranges, entire.first, entire.last);
	}

	pdp->chunkcnt = 0;
	pmemlog_walk(plp, pdp->chunksize, pmempool_dump_log_process_chunk, pdp);

	pmemlog_close(plp);

	return 0;
}

/*
 * pmempool_dump_blk -- dump data from pmem blk pool
 */
static int
pmempool_dump_blk(struct pmempool_dump *pdp)
{
	PMEMblkpool *pbp = pmemblk_open(pdp->fname, pdp->bsize);
	if (!pbp) {
		warn("%s", pdp->fname);
		return -1;
	}

	struct range entire;
	entire.first = 0;
	entire.last = pmemblk_nblock(pbp);

	if (pdp->str_ranges) {
		if (util_parse_ranges(pdp->str_ranges, &pdp->ranges, &entire)
				!= 0) {
			out_err("Cannot parse range\n");
			pmemblk_close(pbp);
			return -1;
		}
	} else {
		util_ranges_add(&pdp->ranges, entire.first, entire.last);
	}

	uint8_t *buff = malloc(pdp->bsize);
	if (!buff)
		err(1, "Cannot allocate memory for pmemblk block buffer");

	int ret = 0;

	uint64_t i;
	struct range *curp = NULL;
	LIST_FOREACH(curp, &pdp->ranges.head, next) {
		for (i = curp->first; i <= curp->last; i++) {
			if (pmemblk_read(pbp, buff, i)) {
				ret = -1;
				out_err("reading block number %lu failed\n", i);
				break;
			}

			if (pdp->hex) {
				uint64_t offset = i * pdp->bsize;
				outv_hexdump(0, buff, pdp->bsize, offset, 0);
			} else {
				if (fwrite(buff, pdp->bsize, 1,
							pdp->ofh) != 1) {
					warn("write");
					ret = -1;
					break;
				}
			}
		}
	}

	free(buff);
	pmemblk_close(pbp);

	return ret;
}

/*
 * pmempool_dump_func -- dump command main function
 */
int
pmempool_dump_func(char *appname, int argc, char *argv[])
{
	struct pmempool_dump pd = pmempool_dump_default;
	LIST_INIT(&pd.ranges.head);

	out_set_vlevel(0);

	int ret = 0;
	int opt;
	while ((opt = getopt_long(argc, argv, "?o:br:c:",
				long_options, NULL)) != -1) {
		switch (opt) {
		case 'o':
			pd.ofname = optarg;
			break;
		case 'b':
			pd.hex = 0;
			break;
		case 'r':
			pd.str_ranges = optarg;
			break;
		case 'c':
			pd.chunksize = atoll(optarg);
			break;
		case '?':
			if (optopt == '\0') {
				pmempool_dump_help(appname);
				exit(EXIT_SUCCESS);
			}
			print_usage(appname);
			exit(EXIT_FAILURE);
		default:
			print_usage(appname);
			exit(EXIT_FAILURE);
		}
	}

	if (optind < argc) {
		pd.fname = argv[optind];
	} else {
		print_usage(appname);
		exit(EXIT_FAILURE);
	}

	if ((pd.fd = open(pd.fname, O_RDWR)) < 0) {
		warn("%s", pd.fname);
		exit(EXIT_FAILURE);
	}

	if (pd.ofname == NULL) {
		/* use standard output by default */
		pd.ofh = stdout;
	} else {
		pd.ofh = fopen(pd.ofname, "wb");
		if (!pd.ofh) {
			warn("%s", pd.ofname);
			exit(EXIT_FAILURE);
		}
	}

	/* set output stream - stdout or file passed by -o option */
	out_set_stream(pd.ofh);

	/* parse pool type and block size for pmem blk pool */
	pmem_pool_type_t type = pmem_pool_parse_params(pd.fname, NULL,
			&pd.bsize);

	switch (type) {
	case PMEM_POOL_TYPE_LOG:
		ret = pmempool_dump_log(&pd);
		break;
	case PMEM_POOL_TYPE_BLK:
		ret = pmempool_dump_blk(&pd);
		break;
	default:
		out_err("%s: pool file corrupted\n", pd.fname);
		ret = -1;
	}

	if (ret)
		out_err("dumping pool file failed\n");

	close(pd.fd);

	if (pd.ofh != stdout)
		fclose(pd.ofh);

	util_ranges_clear(&pd.ranges);

	return ret;
}

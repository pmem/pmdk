// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2014-2023, Intel Corporation */

/*
 * create.c -- pmempool create command source file
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <err.h>
#include "common.h"
#include "dump.h"
#include "output.h"
#include "os.h"
#include "libpmemblk.h"
#include "libpmemlog.h"

#define VERBOSE_DEFAULT	1

/*
 * pmempool_dump -- context and arguments for dump command
 */
struct pmempool_dump {
	char *fname;
	char *ofname;
	char *range;
	FILE *ofh;
	int hex;
	uint64_t bsize;
	struct ranges ranges;
	size_t chunksize;
	uint64_t chunkcnt;
};

/*
 * pmempool_dump_default -- default arguments and context values
 */
static const struct pmempool_dump pmempool_dump_default = {
	.fname		= NULL,
	.ofname		= NULL,
	.range		= NULL,
	.ofh		= NULL,
	.hex		= 1,
	.bsize		= 0,
	.chunksize	= 0,
	.chunkcnt	= 0,
};

/*
 * long_options -- command line options
 */
static const struct option long_options[] = {
	{"output",	required_argument,	NULL,	'o' | OPT_ALL},
	{"binary",	no_argument,		NULL,	'b' | OPT_ALL},
	{"range",	required_argument,	NULL,	'r' | OPT_ALL},
	{"chunk",	required_argument,	NULL,	'c' | OPT_LOG},
	{"help",	no_argument,		NULL,	'h' | OPT_ALL},
	{NULL,		0,			NULL,	 0 },
};

/*
 * help_str -- string for help message
 */
static const char * const help_str =
"Dump user data from pool\n"
"NOTE: pmem blk/log pools are deprecated\n"
"\n"
"Available options:\n"
"  -o, --output <file>  output file name\n"
"  -b, --binary         dump data in binary format\n"
"  -r, --range <range>  range of bytes/blocks/data chunks\n"
"  -c, --chunk <size>   size of chunk for PMEMLOG pool\n"
"  -h, --help           display this help and exit\n"
"\n"
"For complete documentation see %s-dump(1) manual page.\n"
;

/*
 * print_usage -- print application usage short description
 */
static void
print_usage(const char *appname)
{
	printf("Usage: %s dump [<args>] <file>\n", appname);
}

/*
 * print_version -- print version string
 */
static void
print_version(const char *appname)
{
	printf("NOTE: pmem blk/log pools are deprecated\n");
	printf("%s %s\n", appname, SRCVERSION);
}

/*
 * pmempool_dump_help -- print help message for dump command
 */
void
pmempool_dump_help(const char *appname)
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
		PMDK_LIST_FOREACH(curp, &pdp->ranges.head, next) {
			if (pdp->chunkcnt >= curp->first &&
			    pdp->chunkcnt <= curp->last &&
			    pdp->chunksize <= len) {
				if (pdp->hex) {
					outv_hexdump(VERBOSE_DEFAULT,
						buf, pdp->chunksize,
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
		PMDK_LIST_FOREACH(curp, &pdp->ranges.head, next) {
			if (curp->first >= len)
				continue;
			uint8_t *ptr = (uint8_t *)buf + curp->first;
			if (curp->last >= len)
				curp->last = len - 1;
			uint64_t count = curp->last - curp->first + 1;
			if (pdp->hex) {
				outv_hexdump(VERBOSE_DEFAULT, ptr,
						count, curp->first, 0);
			} else {
				if (fwrite(ptr, count, 1, pdp->ofh) != 1)
					err(1, "%s", pdp->ofname);
			}
		}
	}

	return 1;
}

/*
 * pmempool_dump_parse_range -- parse range passed by arguments
 */
static int
pmempool_dump_parse_range(struct pmempool_dump *pdp, size_t max)
{
	struct range entire;
	memset(&entire, 0, sizeof(entire));

	entire.last = max;

	if (util_parse_ranges(pdp->range, &pdp->ranges, entire)) {
		outv_err("invalid range value specified"
				" -- '%s'\n", pdp->range);
		return -1;
	}

	if (PMDK_LIST_EMPTY(&pdp->ranges.head))
		util_ranges_add(&pdp->ranges, entire);

	return 0;
}

/*
 * pmempool_dump_log (DEPRECATED) -- dump data from pmem log pool
 */
static int
pmempool_dump_log(struct pmempool_dump *pdp)
{
	PMEMlogpool *plp = pmemlog_open(pdp->fname);
	if (!plp) {
		warn("%s", pdp->fname);
		return -1;
	}

	os_off_t off = pmemlog_tell(plp);
	if (off < 0) {
		warn("%s", pdp->fname);
		pmemlog_close(plp);
		return -1;
	}

	if (off == 0)
		goto end;

	size_t max = (size_t)off - 1;
	if (pdp->chunksize)
		max /= pdp->chunksize;

	if (pmempool_dump_parse_range(pdp, max))
		return -1;

	pdp->chunkcnt = 0;
	pmemlog_walk(plp, pdp->chunksize, pmempool_dump_log_process_chunk, pdp);

end:
	pmemlog_close(plp);

	return 0;
}

/*
 * pmempool_dump_blk (DEPRECATED) -- dump data from pmem blk pool
 */
static int
pmempool_dump_blk(struct pmempool_dump *pdp)
{
	PMEMblkpool *pbp = pmemblk_open(pdp->fname, pdp->bsize);
	if (!pbp) {
		warn("%s", pdp->fname);
		return -1;
	}

	if (pmempool_dump_parse_range(pdp, pmemblk_nblock(pbp) - 1))
		return -1;

	uint8_t *buff = malloc(pdp->bsize);
	if (!buff)
		err(1, "Cannot allocate memory for pmemblk block buffer");

	int ret = 0;

	uint64_t i;
	struct range *curp = NULL;
	PMDK_LIST_FOREACH(curp, &pdp->ranges.head, next) {
		assert((os_off_t)curp->last >= 0);
		for (i = curp->first; i <= curp->last; i++) {
			if (pmemblk_read(pbp, buff, (os_off_t)i)) {
				ret = -1;
				outv_err("reading block number %lu "
					"failed\n", i);
				break;
			}

			if (pdp->hex) {
				uint64_t offset = i * pdp->bsize;
				outv_hexdump(VERBOSE_DEFAULT, buff,
						pdp->bsize, offset, 0);
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

static const struct option_requirement option_requirements[] = {
	{ 0,  0, 0}
};

/*
 * pmempool_dump_func -- dump command main function
 */
int
pmempool_dump_func(const char *appname, int argc, char *argv[])
{
	struct pmempool_dump pd = pmempool_dump_default;
	PMDK_LIST_INIT(&pd.ranges.head);
	out_set_vlevel(VERBOSE_DEFAULT);

	struct options *opts = util_options_alloc(long_options,
				sizeof(long_options) / sizeof(long_options[0]),
				option_requirements);
	int ret = 0;
	long long chunksize;
	int opt;
	while ((opt = util_options_getopt(argc, argv,
			"ho:br:c:", opts)) != -1) {
		switch (opt) {
		case 'o':
			pd.ofname = optarg;
			break;
		case 'b':
			pd.hex = 0;
			break;
		case 'r':
			pd.range = optarg;
			break;
		case 'c':
			chunksize = atoll(optarg);
			if (chunksize <= 0) {
				outv_err("invalid chunk size specified '%s'\n",
						optarg);
				exit(EXIT_FAILURE);
			}
			pd.chunksize = (size_t)chunksize;
			break;
		case 'h':
			pmempool_dump_help(appname);
			exit(EXIT_SUCCESS);
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

	if (pd.ofname == NULL) {
		/* use standard output by default */
		pd.ofh = stdout;
	} else {
		pd.ofh = os_fopen(pd.ofname, "wb");
		if (!pd.ofh) {
			warn("%s", pd.ofname);
			exit(EXIT_FAILURE);
		}
	}

	/* set output stream - stdout or file passed by -o option */
	out_set_stream(pd.ofh);

	struct pmem_pool_params params;
	/* parse pool type and block size for pmem blk pool */
	pmem_pool_parse_params(pd.fname, &params, 1);

	ret = util_options_verify(opts, params.type);
	if (ret)
		goto out;

	switch (params.type) {
	case PMEM_POOL_TYPE_LOG: /* deprecated */
		ret = pmempool_dump_log(&pd);
		break;
	case PMEM_POOL_TYPE_BLK: /* deprecated */
		pd.bsize = params.blk.bsize;
		ret = pmempool_dump_blk(&pd);
		break;
	case PMEM_POOL_TYPE_OBJ:
		outv_err("%s: PMEMOBJ pool not supported\n", pd.fname);
		ret = -1;
		goto out;
	case PMEM_POOL_TYPE_UNKNOWN:
		outv_err("%s: unknown pool type -- '%s'\n", pd.fname,
				params.signature);
		ret = -1;
		goto out;
	default:
		outv_err("%s: cannot determine type of pool\n", pd.fname);
		ret = -1;
		goto out;
	}

	if (ret)
		outv_err("%s: dumping pool file failed\n", pd.fname);

out:
	if (pd.ofh != stdout)
		fclose(pd.ofh);

	util_ranges_clear(&pd.ranges);

	util_options_free(opts);

	return ret;
}

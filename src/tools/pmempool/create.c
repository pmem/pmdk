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
#include <getopt.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <errno.h>
#include <libgen.h>
#include <err.h>
#include "common.h"
#include "output.h"
#include "libpmemblk.h"
#include "libpmemlog.h"


#define	DEFAULT_MODE	0664
/*
 * pmempool_create -- context and args for create command
 */
struct pmempool_create {
	int verbose;
	char *fname;
	int fexists;
	char *inherit_fname;
	int max_size;
	char *str_type;
	pmem_pool_type_t type;
	uint64_t size;
	char *str_bsize;
	uint64_t bsize;
	uint64_t csize;
	off_t layout;
	mode_t mode;
};

/*
 * pmempool_create_default -- default args for create command
 */
const struct pmempool_create pmempool_create_default = {
	.verbose	= 0,
	.fname		= NULL,
	.fexists	= 0,
	.inherit_fname	= NULL,
	.max_size	= 0,
	.str_type	= NULL,
	.type		= PMEM_POOL_TYPE_NONE,
	.size		= 0,
	.str_bsize	= NULL,
	.bsize		= 0,
	.csize		= 0,
	.layout		= -1,
	.mode		= DEFAULT_MODE,
};

/*
 * help_str -- string for help message
 */
static const char *help_str =
"Create pmem pool of specified size, type and name\n"
"\n"
"Common options:\n"
"  -s, --size  <size>   size of pool\n"
"  -M, --max-size       use maximum available space on file system\n"
"  -m, --mode <octal>   set permissions to <octal> (the default is 0664)\n"
"  -i, --inherit <file> take required parameters from specified pool file\n"
"  -v, --verbose        increase verbosity level\n"
"  -?, --help           display this help and exit\n"
"\n"
"Options for PMEMBLK:\n"
"  -l, --layout <num>   force writing BTT layout using <num> block\n"
"\n"
"For complete documentation see %s-create(1) manual page.\n"
;

/*
 * long_options -- command line options
 */
static const struct option long_options[] = {
	{"size",	required_argument,	0,	's'},
	{"verbose",	no_argument,		0,	'v'},
	{"help",	no_argument,		0,	'?'},
	{"max-size",	no_argument,		0,	'M'},
	{"layout",	optional_argument,	0,	'l'},
	{"inherit",	required_argument,	0,	'i'},
	{"mode",	required_argument,	0,	'm'},
	{0,		0,			0,	 0 },
};

/*
 * print_usage -- print application usage short description
 */
static void
print_usage(char *appname)
{
	printf("Usage: %s create [<args>] <blk|log> [<bsize>] <file>\n",
			appname);
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
 * pmempool_create_help -- print help message for create command
 */
void
pmempool_create_help(char *appname)
{
	print_usage(appname);
	print_version(appname);
	printf(help_str, appname);
}

/*
 * pmempool_create_blk -- create pmem blk pool
 */
int
pmempool_create_blk(struct pmempool_create *pcp)
{
	int ret = 0;

	if (pcp->bsize == 0) {
		outv(1, "No block size option passed"
				" - picking minimum block size.\n");
		pcp->bsize = PMEMBLK_MIN_BLK;
	}

	outv(1, "Creating pmem blk pool with block size %s\n",
		out_get_size_str(pcp->bsize, 1));
	PMEMblkpool *pbp = pmemblk_create(pcp->fname,
			pcp->bsize, pcp->size, pcp->mode);

	if (!pbp) {
		if (pcp->fexists)
			out_err("'%s' -- file exists and not filled by zeros\n",
				pcp->fname);
		else if (util_check_bsize(pcp->bsize, pcp->size))
			out_err("'%lu' -- block size must be < %u\n",
				pcp->bsize,
				util_get_max_bsize(pcp->size));
		else
			warn("%s", pcp->fname);
		return -1;
	}
	size_t nblock = pmemblk_nblock(pbp);
	if (pcp->layout >= 0) {
		if (pcp->layout >= nblock) {
			out_err("'%ld' -- block number must be < %ld\n",
				pcp->layout, nblock);
			ret = -1;
		} else {
			outv(1, "Writing BTT layout using block %lu.\n",
					pcp->layout);

			if (pmemblk_set_error(pbp, pcp->layout) ||
				pmemblk_set_zero(pbp, pcp->layout)) {
				out_err("writing BTT layout to block"
					" %ld failed\n", pcp->layout);
				ret = -1;
			}
		}
	}

	pmemblk_close(pbp);

	return ret;
}

/*
 * pmempool_create_log -- create pmem log pool
 */
int
pmempool_create_log(struct pmempool_create *pcp)
{
	outv(1, "Creating pmem log pool\n");
	PMEMlogpool *plp = pmemlog_create(pcp->fname,
					pcp->size, pcp->mode);

	if (!plp) {
		if (pcp->fexists)
			out_err("'%s' -- file exists and not filled by zeros\n",
				pcp->fname);
		else
			warn("%s", pcp->fname);
		return -1;
	}

	pmemlog_close(plp);

	return 0;
}

/*
 * pmempool_get_max_size -- return maximum allowed size of file
 */
int
pmempool_get_max_size(char *fname, uint64_t *sizep)
{
	struct statvfs buf;
	char *name = strdup(fname);
	char *dir = dirname(name);
	int ret = 0;

	if (statvfs(dir, &buf))
		ret = -1;
	else
		*sizep = buf.f_bsize * buf.f_bavail;

	free(name);

	return ret;
}

/*
 * pmempool_create_parse_args -- parse command line args
 */
static int
pmempool_create_parse_args(struct pmempool_create *pcp, char *appname,
		int argc, char *argv[])
{
	int opt;
	while ((opt = getopt_long(argc, argv, "hvi:s:Mm:l::",
			long_options, NULL)) != -1) {
		switch (opt) {
		case 'v':
			pcp->verbose = 1;
			break;
		case '?':
			pmempool_create_help(appname);
			exit(EXIT_SUCCESS);
		case 's':
			if (util_parse_size(optarg, &pcp->size)) {
				out_err("cannot parse '%s' as size\n", optarg);
				return -1;
			}
			break;
		case 'M':
			pcp->max_size = 1;
			break;
		case 'm':
			if (sscanf(optarg, "%o", &pcp->mode) != 1) {
				out_err("cannot parse mode\n");
				return -1;
			}
			break;
		case 'i':
			pcp->inherit_fname = optarg;
			break;
		case 'l':
			if (optarg)
				pcp->layout = atoll(optarg);
			else
				pcp->layout = 0;
			break;
		default:
			print_usage(appname);
			exit(EXIT_FAILURE);
		}
	}

	/* check for <type>, <bsize> and <file> strings */
	if (optind + 2 < argc) {
		pcp->str_type = argv[optind];
		pcp->str_bsize = argv[optind + 1];
		pcp->fname = argv[optind + 2];
	} else if (optind + 1 < argc) {
		pcp->str_type = argv[optind];
		pcp->fname = argv[optind + 1];
	} else if (optind < argc) {
		pcp->fname = argv[optind];
		pcp->str_type = NULL;
	} else {
		print_usage(appname);
		exit(EXIT_FAILURE);
	}

	return 0;
}

/*
 * pmempool_create_func -- main function for create command
 */
int
pmempool_create_func(char *appname, int argc, char *argv[])
{
	int ret = 0;
	struct pmempool_create pc = pmempool_create_default;

	/* parse command line arguments */
	ret = pmempool_create_parse_args(&pc, appname, argc, argv);
	if (ret)
		return ret;

	/* set verbosity level */
	out_set_vlevel(pc.verbose);

	umask(0);

	/*
	 * Parse pool type and other parameters if --inherit option
	 * passed. It is possible to either pass --inherit option
	 * or pool type string in command line arguments. This is
	 * validated here.
	 */
	if (pc.str_type) {
		/* parse pool type string if passed in command line arguments */
		pc.type = pmem_pool_type_parse_str(pc.str_type);
		if (PMEM_POOL_TYPE_UNKNWON == pc.type) {
			out_err("'%s' -- unknown pool type\n", pc.str_type);
			return -1;
		}

		if (PMEM_POOL_TYPE_BLK == pc.type) {
			if (pc.str_bsize == NULL) {
				out_err("blk pool requires <bsize> argument\n");
				return -1;
			}
			if (util_parse_size(pc.str_bsize, &pc.bsize)) {
				out_err("cannot parse '%s' as block size\n",
						pc.str_bsize);
				return -1;
			}
		}

	} else if (pc.inherit_fname) {
		/*
		 * If no type string passed, --inherit option must be passed
		 * so parse file and get required parameters.
		 */
		outv(1, "Parsing '%s' file:\n", pc.inherit_fname);
		pc.type = pmem_pool_parse_params(pc.inherit_fname, &pc.size,
				&pc.bsize);
		if (PMEM_POOL_TYPE_UNKNWON == pc.type) {
			out_err("'%s' -- unknown pool type\n",
					pc.inherit_fname);
			return -1;
		} else {
			outv(1, "  type : %s\n",
					out_get_pool_type_str(pc.type));
			outv(1, "  size : %s\n",
					out_get_size_str(pc.size, 2));
			if (pc.type == PMEM_POOL_TYPE_BLK)
				outv(1, "  bsize: %s\n",
					out_get_size_str(pc.bsize, 0));
		}
	} else {
		/* neither pool type string nor --inherit options passed */
		print_usage(appname);
		return -1;
	}

	if (pc.size && pc.max_size) {
		out_err("'-M' option cannot be used with '-s'"
				" option\n");
		return -1;
	}

	pc.fexists = access(pc.fname, F_OK) == 0;
	/*
	 * If neither --size nor --inherit options passed, check
	 * for --max-size option - if not passed use minimum pool size.
	 */
	uint64_t min_size = pmem_pool_get_min_size(pc.type);
	if (pc.size == 0) {
		if (pc.max_size) {
			outv(1, "Maximum size option passed "
				"- getting available space of file system.\n");
			int ret = pmempool_get_max_size(pc.fname, &pc.size);
			if (ret) {
				out_err("cannot get available space of fs\n");
				return -1;
			}
			if (pc.size == 0) {
				out_err("No space left on device\n");
				return -1;
			}
			outv(1, "Available space is %s\n",
				out_get_size_str(pc.size, 2));
		} else {
			if (!pc.fexists) {
				outv(1, "No size option passed "
					"- picking minimum pool size.\n");
				pc.size = min_size;
			}
		}
	} else {
		if (pc.size < min_size) {
			out_err("size must be >= %lu bytes\n", min_size);
			return -1;
		}
	}

	switch (pc.type) {
	case PMEM_POOL_TYPE_BLK:
		ret = pmempool_create_blk(&pc);
		break;
	case PMEM_POOL_TYPE_LOG:
		ret = pmempool_create_log(&pc);
		break;
	default:
		break;
	}

	if (ret) {
		out_err("creating pool file failed\n");
		if (!pc.fexists)
			remove(pc.fname);
	}

	return ret;
}

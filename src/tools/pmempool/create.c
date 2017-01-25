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
#include "file.h"
#include "create.h"

#include "set.h"
#include "output.h"
#include "libpmemblk.h"
#include "libpmemlog.h"


#define DEFAULT_MODE	0664
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
	struct pmem_pool_params params;
	struct pmem_pool_params inherit_params;
	char *str_size;
	char *str_mode;
	char *str_bsize;
	uint64_t csize;
	int write_btt_layout;
	char *layout;
	struct options *opts;
};

/*
 * pmempool_create_default -- default args for create command
 */
static const struct pmempool_create pmempool_create_default = {
	.verbose	= 0,
	.fname		= NULL,
	.fexists	= 0,
	.inherit_fname	= NULL,
	.max_size	= 0,
	.str_type	= NULL,
	.str_bsize	= NULL,
	.csize		= 0,
	.write_btt_layout = 0,
	.layout		= NULL,
	.params		= {
		.type	= PMEM_POOL_TYPE_UNKNOWN,
		.size	= 0,
		.mode	= DEFAULT_MODE,
	}
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
"  -h, --help           display this help and exit\n"
"\n"
"Options for PMEMBLK:\n"
"  -w, --write-layout force writing the BTT layout\n"
"\n"
"Options for PMEMOBJ:\n"
"  -l, --layout <name>  layout name stored in pool's header\n"
"\n"
"For complete documentation see %s-create(1) manual page.\n"
;

/*
 * long_options -- command line options
 */
static const struct option long_options[] = {
	{"size",	required_argument,	0,	's' | OPT_ALL},
	{"verbose",	no_argument,		0,	'v' | OPT_ALL},
	{"help",	no_argument,		0,	'h' | OPT_ALL},
	{"max-size",	no_argument,		0,	'M' | OPT_ALL},
	{"inherit",	required_argument,	0,	'i' | OPT_ALL},
	{"mode",	required_argument,	0,	'm' | OPT_ALL},
	{"write-layout", no_argument,		0,	'w' | OPT_BLK},
	{"layout",	required_argument,	0,	'l' | OPT_OBJ},
	{0,		0,			0,	 0 },
};

/*
 * print_usage -- print application usage short description
 */
static void
print_usage(char *appname)
{
	printf("Usage: %s create [<args>] <blk|log|obj> [<bsize>] <file>\n",
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
 * pmempool_create_obj -- create pmem obj pool
 */
static int
pmempool_create_obj(struct pmempool_create *pcp)
{
	PMEMobjpool *pop = pmemobj_create(pcp->fname, pcp->layout,
			pcp->params.size, pcp->params.mode);
	if (!pop) {
		outv_err("'%s' -- %s\n", pcp->fname, pmemobj_errormsg());
		return -1;
	}

	pmemobj_close(pop);

	return 0;
}

/*
 * pmempool_create_blk -- create pmem blk pool
 */
static int
pmempool_create_blk(struct pmempool_create *pcp)
{
	int ret = 0;

	if (pcp->params.blk.bsize == 0) {
		outv(1, "No block size option passed"
				" - picking minimum block size.\n");
		pcp->params.blk.bsize = PMEMBLK_MIN_BLK;
	}

	PMEMblkpool *pbp = pmemblk_create(pcp->fname, pcp->params.blk.bsize,
			pcp->params.size, pcp->params.mode);
	if (!pbp) {
		outv_err("'%s' -- %s\n", pcp->fname, pmemblk_errormsg());
		return -1;
	}

	if (pcp->write_btt_layout) {
		outv(1, "Writing BTT layout using block %lu.\n",
				pcp->write_btt_layout);

		if (pmemblk_set_error(pbp, 0) || pmemblk_set_zero(pbp, 0)) {
			outv_err("writing BTT layout to block 0 failed\n");
			ret = -1;
		}
	}

	pmemblk_close(pbp);

	return ret;
}

/*
 * pmempool_create_log -- create pmem log pool
 */
static int
pmempool_create_log(struct pmempool_create *pcp)
{
	PMEMlogpool *plp = pmemlog_create(pcp->fname,
					pcp->params.size, pcp->params.mode);

	if (!plp) {
		outv_err("'%s' -- %s\n", pcp->fname, pmemlog_errormsg());
		return -1;
	}

	pmemlog_close(plp);

	return 0;
}

/*
 * pmempool_get_max_size -- return maximum allowed size of file
 */
#ifndef _WIN32
static int
pmempool_get_max_size(const char *fname, uint64_t *sizep)
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
#else
static int
pmempool_get_max_size(const char *fname, uint64_t *sizep)
{
	char *name = strdup(fname);
	char *dir = dirname(name);
	int ret = 0;
	ULARGE_INTEGER freespace;
	if (GetDiskFreeSpaceExA(dir, &freespace, NULL, NULL))
		ret = -1;
	else
		*sizep = freespace.QuadPart;

	free(name);

	return ret;
}
#endif

/*
 * print_pool_params -- print some parameters of a pool
 */
static void
print_pool_params(struct pmem_pool_params *params)
{
	outv(1, "\ttype  : %s\n", out_get_pool_type_str(params->type));
	outv(1, "\tsize  : %s\n", out_get_size_str(params->size, 2));
	outv(1, "\tmode  : 0%o\n", params->mode);
	switch (params->type) {
	case PMEM_POOL_TYPE_BLK:
		outv(1, "\tbsize : %s\n",
			out_get_size_str(params->blk.bsize, 0));
		break;
	case PMEM_POOL_TYPE_OBJ:
		outv(1, "\tlayout: '%s'\n", params->obj.layout);
		break;
	default:
		break;
	}
}

/*
 * inherit_pool_params -- inherit pool parameters from specified file
 */
static int
inherit_pool_params(struct pmempool_create *pcp)
{
	outv(1, "Parsing pool: '%s'\n", pcp->inherit_fname);

	/*
	 * If no type string passed, --inherit option must be passed
	 * so parse file and get required parameters.
	 */
	if (pmem_pool_parse_params(pcp->inherit_fname,
			&pcp->inherit_params, 1)) {
		if (errno)
			perror(pcp->inherit_fname);
		else
			outv_err("%s: cannot determine type of pool\n",
				pcp->inherit_fname);
		return -1;
	}

	if (PMEM_POOL_TYPE_UNKNOWN == pcp->inherit_params.type) {
		outv_err("'%s' -- unknown pool type\n",
				pcp->inherit_fname);
		return -1;
	}

	print_pool_params(&pcp->inherit_params);

	return 0;
}

/*
 * pmempool_create_parse_args -- parse command line args
 */
static int
pmempool_create_parse_args(struct pmempool_create *pcp, char *appname,
		int argc, char *argv[], struct options *opts)
{
	int opt, ret;
	while ((opt = util_options_getopt(argc, argv, "vhi:s:Mm:l:w",
			opts)) != -1) {
		switch (opt) {
		case 'v':
			pcp->verbose = 1;
			break;
		case 'h':
			pmempool_create_help(appname);
			exit(EXIT_SUCCESS);
		case 's':
			pcp->str_size = optarg;
			ret = util_parse_size(optarg, &pcp->params.size);
			if (ret || pcp->params.size == 0) {
				outv_err("invalid size value specified '%s'\n",
						optarg);
				return -1;
			}
			break;
		case 'M':
			pcp->max_size = 1;
			break;
		case 'm':
			pcp->str_mode = optarg;
			if (util_parse_mode(optarg, &pcp->params.mode)) {
				outv_err("invalid mode value specified '%s'\n",
						optarg);
				return -1;
			}
			break;
		case 'i':
			pcp->inherit_fname = optarg;
			break;
		case 'w':
			pcp->write_btt_layout = 1;
			break;
		case 'l':
			pcp->layout = optarg;
			break;
		default:
			print_usage(appname);
			return -1;
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
		return -1;
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
	pc.opts = util_options_alloc(long_options, sizeof(long_options) /
			sizeof(long_options[0]), NULL);

	/* parse command line arguments */
	ret = pmempool_create_parse_args(&pc, appname, argc, argv, pc.opts);
	if (ret)
		exit(EXIT_FAILURE);

	/* set verbosity level */
	out_set_vlevel(pc.verbose);

	umask(0);

	pc.fexists = access(pc.fname, F_OK) == 0;
	int is_poolset = util_is_poolset_file(pc.fname) == 1;

	if (pc.inherit_fname)  {
		if (inherit_pool_params(&pc)) {
			outv_err("parsing pool '%s' failed\n",
					pc.inherit_fname);
			return -1;
		}
	}

	/*
	 * Parse pool type and other parameters if --inherit option
	 * passed. It is possible to either pass --inherit option
	 * or pool type string in command line arguments. This is
	 * validated here.
	 */
	if (pc.str_type) {
		/* parse pool type string if passed in command line arguments */
		pc.params.type = pmem_pool_type_parse_str(pc.str_type);
		if (PMEM_POOL_TYPE_UNKNOWN == pc.params.type) {
			outv_err("'%s' -- unknown pool type\n", pc.str_type);
			return -1;
		}

		if (PMEM_POOL_TYPE_BLK == pc.params.type) {
			if (pc.str_bsize == NULL) {
				outv_err("blk pool requires <bsize> "
					"argument\n");
				return -1;
			}
			if (util_parse_size(pc.str_bsize,
						&pc.params.blk.bsize)) {
				outv_err("cannot parse '%s' as block size\n",
						pc.str_bsize);
				return -1;
			}
		}
	} else if (pc.inherit_fname) {
		pc.params.type = pc.inherit_params.type;
	} else {
		/* neither pool type string nor --inherit options passed */
		print_usage(appname);
		return -1;
	}

	if (util_options_verify(pc.opts, pc.params.type))
		return -1;

	if (pc.params.type != PMEM_POOL_TYPE_BLK && pc.str_bsize != NULL) {
		outv_err("invalid option specified for %s pool type"
				" -- block size\n",
			out_get_pool_type_str(pc.params.type));
		return -1;
	}

	if (is_poolset) {
		if (pc.params.size) {
			outv_err("-s|--size cannot be used with "
					"poolset file\n");
			return -1;
		}

		if (pc.max_size) {
			outv_err("-M|--max-size cannot be used with "
					"poolset file\n");
			return -1;
		}
	}

	if (pc.params.size && pc.max_size) {
		outv_err("-M|--max-size option cannot be used with -s|--size"
				" option\n");
		return -1;
	}

	if (pc.layout && strlen(pc.layout) >= PMEMOBJ_MAX_LAYOUT) {
		outv_err("Layout name is to long, maximum number of characters"
			" (including the terminating null byte) is %d\n",
			PMEMOBJ_MAX_LAYOUT);
		return -1;
	}

	if (pc.inherit_fname)  {
		if (!pc.str_size && !pc.max_size)
			pc.params.size = pc.inherit_params.size;
		if (!pc.str_mode)
			pc.params.mode = pc.inherit_params.mode;
		switch (pc.params.type) {
		case PMEM_POOL_TYPE_BLK:
			if (!pc.str_bsize)
				pc.params.blk.bsize =
					pc.inherit_params.blk.bsize;
			break;
		case PMEM_POOL_TYPE_OBJ:
			if (!pc.layout) {
				memcpy(pc.params.obj.layout,
					pc.inherit_params.obj.layout,
					sizeof(pc.params.obj.layout));
			} else {
				size_t len = sizeof(pc.params.obj.layout);
				strncpy(pc.params.obj.layout, pc.layout,
						len - 1);
				pc.params.obj.layout[len - 1] = '\0';
			}
			break;
		default:
			break;
		}
	}

	/*
	 * If neither --size nor --inherit options passed, check
	 * for --max-size option - if not passed use minimum pool size.
	 */
	uint64_t min_size = pmem_pool_get_min_size(pc.params.type);
	if (pc.params.size == 0) {
		if (pc.max_size) {
			outv(1, "Maximum size option passed "
				"- getting available space of file system.\n");
			ret = pmempool_get_max_size(pc.fname,
					&pc.params.size);
			if (ret) {
				outv_err("cannot get available space of fs\n");
				return -1;
			}
			if (pc.params.size == 0) {
				outv_err("No space left on device\n");
				return -1;
			}
			outv(1, "Available space is %s\n",
				out_get_size_str(pc.params.size, 2));
		} else {
			if (!pc.fexists) {
				outv(1, "No size option passed "
					"- picking minimum pool size.\n");
				pc.params.size = min_size;
			}
		}
	} else {
		if (pc.params.size < min_size) {
			outv_err("size must be >= %lu bytes\n", min_size);
			return -1;
		}
	}


	outv(1, "Creating pool: %s\n", pc.fname);
	print_pool_params(&pc.params);

	switch (pc.params.type) {
	case PMEM_POOL_TYPE_BLK:
		ret = pmempool_create_blk(&pc);
		break;
	case PMEM_POOL_TYPE_LOG:
		ret = pmempool_create_log(&pc);
		break;
	case PMEM_POOL_TYPE_OBJ:
		ret = pmempool_create_obj(&pc);
		break;
	default:
		ret = -1;
		break;
	}

	if (ret) {
		outv_err("creating pool file failed\n");
		if (!pc.fexists)
			util_unlink(pc.fname);
	}

	util_options_free(pc.opts);
	return ret;
}

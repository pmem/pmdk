// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2014-2023, Intel Corporation */

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
#include "os.h"

#include "set.h"
#include "output.h"
#include "libpmempool.h"

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
	uint64_t csize;
	int force;
	char *layout;
	struct options *opts;
	int clearbadblocks;
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
	.csize		= 0,
	.force		= 0,
	.layout		= NULL,
	.clearbadblocks	= 0,
	.params		= {
		.type	= PMEM_POOL_TYPE_UNKNOWN,
		.size	= 0,
		.mode	= DEFAULT_MODE,
	}
};

/*
 * help_str -- string for help message
 */
static const char * const help_str =
"Create pmem pool of specified size, type and name\n"
"\n"
"Common options:\n"
"  -s, --size  <size>   size of pool\n"
"  -M, --max-size       use maximum available space on file system\n"
"  -m, --mode <octal>   set permissions to <octal> (the default is 0664)\n"
"  -i, --inherit <file> take required parameters from specified pool file\n"
"  -b, --clear-bad-blocks clear bad blocks in existing files\n"
"  -f, --force          remove the pool first\n"
"  -v, --verbose        increase verbosity level\n"
"  -h, --help           display this help and exit\n"
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
	{"size",	required_argument,	NULL,	's' | OPT_ALL},
	{"verbose",	no_argument,		NULL,	'v' | OPT_ALL},
	{"help",	no_argument,		NULL,	'h' | OPT_ALL},
	{"max-size",	no_argument,		NULL,	'M' | OPT_ALL},
	{"inherit",	required_argument,	NULL,	'i' | OPT_ALL},
	{"mode",	required_argument,	NULL,	'm' | OPT_ALL},
	{"layout",	required_argument,	NULL,	'l' | OPT_OBJ},
	{"force",	no_argument,		NULL,	'f' | OPT_ALL},
	{"clear-bad-blocks", no_argument,		NULL,	'b' | OPT_ALL},
	{NULL,		0,			NULL,	 0 },
};

/*
 * print_usage -- print application usage short description
 */
static void
print_usage(const char *appname)
{
	printf("Usage: %s create [<args>] [obj] <file>\n", appname);
}

/*
 * print_version -- print version string
 */
static void
print_version(const char *appname)
{
	printf("%s %s\n", appname, SRCVERSION);
}

/*
 * pmempool_create_help -- print help message for create command
 */
void
pmempool_create_help(const char *appname)
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
 * pmempool_get_max_size -- return maximum allowed size of file
 */
static int
pmempool_get_max_size(const char *fname, uint64_t *sizep)
{
	struct statvfs buf;
	int ret = 0;
	char *name = strdup(fname);
	if (name == NULL) {
		return -1;
	}

	char *dir = dirname(name);

	if (statvfs(dir, &buf))
		ret = -1;
	else
		*sizep = buf.f_bsize * buf.f_bavail;

	free(name);

	return ret;
}

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
pmempool_create_parse_args(struct pmempool_create *pcp, const char *appname,
		int argc, char *argv[], struct options *opts)
{
	int opt, ret;
	while ((opt = util_options_getopt(argc, argv, "vhi:s:Mm:l:wfb",
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
			ret = util_parse_size(optarg,
			    (size_t *)&pcp->params.size);
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
		case 'l':
			pcp->layout = optarg;
			break;
		case 'f':
			pcp->force = 1;
			break;
		case 'b':
			pcp->clearbadblocks = 1;
			break;
		default:
			print_usage(appname);
			return -1;
		}
	}

	/* check for <type> and <file> strings */
	if (optind + 2 == argc) {
		pcp->str_type = argv[optind];
		pcp->fname = argv[optind + 1];
	} else if (optind + 1 == argc) {
		pcp->fname = argv[optind];
		pcp->str_type = NULL;
	} else {
		print_usage(appname);
		return -1;
	}

	return 0;
}

static int
allocate_max_size_available_file(const char *name_of_file, mode_t mode,
		os_off_t max_size)
{
	int fd = os_open(name_of_file, O_CREAT | O_EXCL | O_RDWR, mode);
	if (fd == -1) {
		outv_err("!open '%s' failed", name_of_file);
		return -1;
	}

	os_off_t offset = 0;
	os_off_t length = max_size - (max_size % (os_off_t)Pagesize);
	int ret;
	do {
		ret = os_posix_fallocate(fd, offset, length);
		if (ret == 0)
			offset += length;
		else if (ret != ENOSPC) {
			os_close(fd);
			if (os_unlink(name_of_file) == -1)
				outv_err("!unlink '%s' failed", name_of_file);
			errno = ret;
			outv_err("!space allocation for '%s' failed",
					name_of_file);
			return -1;
		}

		length /= 2;
		length -= (length % (os_off_t)Pagesize);
	} while (length > (os_off_t)Pagesize);

	os_close(fd);

	return 0;
}

/*
 * pmempool_create_func -- main function for create command
 */
int
pmempool_create_func(const char *appname, int argc, char *argv[])
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

	int exists = util_file_exists(pc.fname);
	if (exists < 0)
		return -1;

	pc.fexists = exists;
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

		if (PMEM_POOL_TYPE_OBJ == pc.params.type && pc.layout != NULL) {
			size_t max_layout = PMEMOBJ_MAX_LAYOUT;

			if (strlen(pc.layout) >= max_layout) {
				outv_err(
						"Layout name is too long, maximum number of characters (including the terminating null byte) is %zu\n",
						max_layout);
				return -1;
			}

			size_t len = sizeof(pc.params.obj.layout);
			strncpy(pc.params.obj.layout, pc.layout, len);
			pc.params.obj.layout[len - 1] = '\0';
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

	if (pc.inherit_fname)  {
		if (!pc.str_size && !pc.max_size)
			pc.params.size = pc.inherit_params.size;
		if (!pc.str_mode)
			pc.params.mode = pc.inherit_params.mode;
		switch (pc.params.type) {
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
			if (allocate_max_size_available_file(pc.fname,
					pc.params.mode,
					(os_off_t)pc.params.size))
				return -1;
			/*
			 * We are going to create pool based
			 * on file size instead of the pc.params.size.
			 */
			pc.params.size = 0;
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

	if (pc.force)
		pmempool_rm(pc.fname, PMEMPOOL_RM_FORCE);

	outv(1, "Creating pool: %s\n", pc.fname);
	print_pool_params(&pc.params);

	if (pc.clearbadblocks) {
		int ret = util_pool_clear_badblocks(pc.fname,
						1 /* ignore non-existing */);
		if (ret) {
			outv_err("'%s' -- clearing bad blocks failed\n",
					pc.fname);
			return -1;
		}
	}

	switch (pc.params.type) {
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

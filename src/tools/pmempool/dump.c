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
	struct ranges ranges;
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
};

/*
 * long_options -- command line options
 */
static const struct option long_options[] = {
	{"output",	required_argument,	NULL,	'o' | OPT_ALL},
	{"binary",	no_argument,		NULL,	'b' | OPT_ALL},
	{"range",	required_argument,	NULL,	'r' | OPT_ALL},
	{"help",	no_argument,		NULL,	'h' | OPT_ALL},
	{NULL,		0,			NULL,	 0 },
};

/*
 * help_str -- string for help message
 */
static const char * const help_str =
"Dump user data from pool\n"
"\n"
"Available options:\n"
"  -o, --output <file>  output file name\n"
"  -b, --binary         dump data in binary format\n"
"  -r, --range <range>  range of bytes/blocks/data chunks\n"
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
	/* parse pool type */
	pmem_pool_parse_params(pd.fname, &params, 1);

	ret = util_options_verify(opts, params.type);
	if (ret)
		goto out;

	switch (params.type) {
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

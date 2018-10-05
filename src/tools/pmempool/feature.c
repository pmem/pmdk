/*
 * Copyright 2018, Intel Corporation
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
 * feature.c -- pmempool feature command source file
 */
#include <getopt.h>
#include <stdlib.h>

#include "common.h"
#include "feature.h"
#include "output.h"
#include "libpmempool.h"

/* operations over features */
enum feature_op {
	undefined,
	enable,
	disable,
	query
};

/*
 * feature_ctx -- context and arguments for feature command
 */
struct feature_ctx {
	int verbose;
	const char *fname;
	enum feature_op op;
	enum pmempool_feature feature;
	unsigned flags;
};

/*
 * pmempool_feature_default -- default arguments for feature command
 */
static const struct feature_ctx pmempool_feature_default = {
	.verbose	= 0,
	.fname		= NULL,
	.op		= undefined,
	.feature	= UINT32_MAX,
	.flags		= 0
};

/*
 * help_str -- string for help message
 */
static const char * const help_str =
"Toggle or query a pool feature\n"
"\n"
"For complete documentation see %s-feature(1) manual page.\n"
;

/*
 * long_options -- command line options
 */
static const struct option long_options[] = {
	{"enable",	required_argument,	NULL,	'e'},
	{"disable",	required_argument,	NULL,	'd'},
	{"query",	required_argument,	NULL,	'q'},
	{"verbose",	no_argument,		NULL,	'v'},
	{"help",	no_argument,		NULL,	'h'},
	{NULL,		0,			NULL,	 0 },
};

/*
 * print_usage -- print short description of application's usage
 */
static void
print_usage(const char *appname)
{
	printf("Usage: %s feature [<args>] <file>\n", appname);
	printf(
		"feature: SINGLEHDR, CKSUM_2K, SHUTDOWN_STATE, CHECK_BAD_BLOCKS\n");
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
 * pmempool_feature_help -- print help message for feature command
 */
void
pmempool_feature_help(const char *appname)
{
	print_usage(appname);
	print_version(appname);
	printf(help_str, appname);
}

/*
 * feature_perform -- perform operation over function
 */
static int
feature_perform(struct feature_ctx *pfp)
{
	int ret;

	switch (pfp->op) {
	case enable:
		return pmempool_feature_enable(pfp->fname, pfp->feature,
				pfp->flags);
	case disable:
		return pmempool_feature_disable(pfp->fname, pfp->feature,
				pfp->flags);
	case query:
		ret = pmempool_feature_query(pfp->fname, pfp->feature,
				pfp->flags);
		if (ret < 0)
			return 1;
		printf("%d", ret);
		return 0;
	default:
		ERR("Invalid option.");
		return -1;
	}
}

/*
 * set_op -- set operation
 */
static void
set_op(const char *appname, struct feature_ctx *pfp, enum feature_op op,
		const char *feature)
{
	/* only one operation allowed */
	if (pfp->op != undefined)
		goto misuse;
	pfp->op = op;

	/* parse feature name */
	uint32_t fval = util_str2pmempool_feature(feature);
	if (fval == UINT32_MAX)
		goto misuse;
	pfp->feature = (enum pmempool_feature)fval;
	return;

misuse:
	print_usage(appname);
	exit(EXIT_FAILURE);
}

/*
 * parse_args -- parse command line arguments
 */
static int
parse_args(struct feature_ctx *pfp, const char *appname,
		int argc, char *argv[])
{
	int opt;
	while ((opt = getopt_long(argc, argv, "vhe:d:q:h",
			long_options, NULL)) != -1) {
		switch (opt) {
		case 'e':
			set_op(appname, pfp, enable, optarg);
			break;
		case 'd':
			set_op(appname, pfp, disable, optarg);
			break;
		case 'q':
			set_op(appname, pfp, query, optarg);
			break;
		case 'v':
			pfp->verbose = 2;
			break;
		case 'h':
			pmempool_feature_help(appname);
			exit(EXIT_SUCCESS);
		default:
			print_usage(appname);
			exit(EXIT_FAILURE);
		}
	}

	if (optind >= argc) {
		print_usage(appname);
		exit(EXIT_FAILURE);
	}
	pfp->fname = argv[optind];
	return 0;
}

/*
 * pmempool_feature_func -- main function for feature command
 */
int
pmempool_feature_func(const char *appname, int argc, char *argv[])
{
	struct feature_ctx pf = pmempool_feature_default;
	int ret = 0;

	/* parse command line arguments */
	ret = parse_args(&pf, appname, argc, argv);
	if (ret)
		return ret;

	/* set verbosity level */
	out_set_vlevel(pf.verbose);

	return feature_perform(&pf);
}

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

enum feature_op {
	enable,
	disable,
	query,
	undefined
};

/*
 * feature_ctx -- context and args for feature command
 */
struct feature_ctx {
	int verbose;
	const char *fname;
	enum feature_op op;
	uint32_t feature;
};

/*
 * pmempool_feature_default -- default args for feature command
 */
static const struct feature_ctx pmempool_feature_default = {
	.verbose	= 0,
	.fname		= NULL,
	.op		= undefined,
	.feature	= 0
};

/*
 * help_str -- string for help message
 */
static const char * const help_str =
"Toggle or query a pool features\n"
"\n"
"For complete documentation see %s-check(1) manual page.\n"
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
print_usage(char *appname)
{
	printf("Usage: %s feature [<args>] <file>\n", appname);
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
 * pmempool_feature_help -- print help message for feature command
 */
void
pmempool_feature_help(char *appname)
{
	print_usage(appname);
	print_version(appname);
	printf(help_str, appname);
}

/*
 * feature_perform -- XXX
 */
static int
feature_perform(struct feature_ctx *pfp)
{

	switch (pfp->op) {
	case enable:
		return pmempool_feature_enable(pfp->fname, pfp->feature);
	case disable:
		return pmempool_feature_disable(pfp->fname, pfp->feature);
	case query:
		return pmempool_feature_query(pfp->fname, pfp->feature);
	default:
		ERR("Invalid option.");
		return -1;
	}
}

/*
 * set_op -- XXX
 */
static void
set_op(char *appname, struct feature_ctx *pfp, enum feature_op op,
		const char *feature)
{
	if (pfp->op == undefined) {
		pfp->op = op;
		pfp->feature = out_str2feature(feature);
	} else {
		print_usage(appname);
		exit(EXIT_FAILURE);
	}
}

/*
 * parse_args -- parse command line arguments
 */
static int
parse_args(struct feature_ctx *pfp, char *appname,
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

	if (optind < argc) {
		pfp->fname = argv[optind];
	} else {
		print_usage(appname);
		exit(EXIT_FAILURE);
	}

	return 0;
}

/*
 * pmempool_feature_func -- main function for feature command
 */
int
pmempool_feature_func(char *appname, int argc, char *argv[])
{
	int ret = 0;
	struct feature_ctx pf = pmempool_feature_default;

	/* parse command line arguments */
	ret = parse_args(&pf, appname, argc, argv);
	if (ret)
		return ret;

	/* set verbosity level */
	out_set_vlevel(pf.verbose);

	ret = feature_perform(&pf);

	/*
	 * XXX - print some output message
	 */

	return ret;
}

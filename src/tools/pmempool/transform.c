/*
 * Copyright 2016-2018, Intel Corporation
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
 * transform.c -- pmempool transform command source file
 */

#include <stdio.h>
#include <libgen.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <endian.h>
#include "common.h"
#include "output.h"
#include "transform.h"
#include "libpmempool.h"

/*
 * pmempool_transform_context -- context and arguments for transform command
 */
struct pmempool_transform_context {
	unsigned flags;		/* flags which modify the command execution */
	char *poolset_file_src;	/* a path to a source poolset file */
	char *poolset_file_dst;	/* a path to a target poolset file */
};

/*
 * pmempool_transform_default -- default arguments for transform command
 */
static const struct pmempool_transform_context pmempool_transform_default = {
	.flags			= 0,
	.poolset_file_src	= NULL,
	.poolset_file_dst	= NULL,
};

/*
 * help_str -- string for help message
 */
static const char * const help_str =
"Modify internal structure of a poolset\n"
"\n"
"Common options:\n"
"  -d, --dry-run        do not apply changes, only check for viability of"
" transformation\n"
"  -v, --verbose        increase verbosity level\n"
"  -h, --help           display this help and exit\n"
"\n"
"For complete documentation see %s-transform(1) manual page.\n"
;

/*
 * long_options -- command line options
 */
static const struct option long_options[] = {
	{"dry-run",	no_argument,		NULL,	'd'},
	{"help",	no_argument,		NULL,	'h'},
	{"verbose",	no_argument,		NULL,	'v'},
	{NULL,		0,			NULL,	 0 },
};

/*
 * print_usage -- print application usage short description
 */
static void
print_usage(char *appname)
{
	printf("usage: %s transform [<options>] <poolset_file_src>"
			" <poolset_file_dst>\n", appname);
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
 * pmempool_transform_help -- print help message for the transform command
 */
void
pmempool_transform_help(char *appname)
{
	print_usage(appname);
	print_version(appname);
	printf(help_str, appname);
}

/*
 * pmempool_check_parse_args -- parse command line arguments
 */
static int
pmempool_transform_parse_args(struct pmempool_transform_context *ctx,
		char *appname, int argc, char *argv[])
{
	int opt;
	while ((opt = getopt_long(argc, argv, "dhv",
			long_options, NULL)) != -1) {
		switch (opt) {
		case 'd':
			ctx->flags = PMEMPOOL_TRANSFORM_DRY_RUN;
			break;
		case 'h':
			pmempool_transform_help(appname);
			exit(EXIT_SUCCESS);
		case 'v':
			out_set_vlevel(1);
			break;
		default:
			print_usage(appname);
			exit(EXIT_FAILURE);
		}
	}

	if (optind + 1 < argc) {
		ctx->poolset_file_src = argv[optind];
		ctx->poolset_file_dst = argv[optind + 1];
	} else {
		print_usage(appname);
		exit(EXIT_FAILURE);
	}

	return 0;
}

/*
 * pmempool_transform_func -- main function for the transform command
 */
int
pmempool_transform_func(char *appname, int argc, char *argv[])
{
	int ret;
	struct pmempool_transform_context ctx = pmempool_transform_default;

	/* parse command line arguments */
	if ((ret = pmempool_transform_parse_args(&ctx, appname, argc, argv)))
		return ret;

	ret = pmempool_transform(ctx.poolset_file_src, ctx.poolset_file_dst,
			ctx.flags);

	if (ret) {
		if (errno)
			outv_err("%s\n", strerror(errno));
		outv_err("failed to transform %s -> %s: %s\n",
				ctx.poolset_file_src, ctx.poolset_file_dst,
				pmempool_errormsg());
		return -1;
	} else {
		outv(1, "%s -> %s: transformed\n", ctx.poolset_file_src,
				ctx.poolset_file_dst);
		return 0;
	}
}

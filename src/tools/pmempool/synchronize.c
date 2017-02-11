/*
 * Copyright 2016, Intel Corporation
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
 * synchronize.c -- pmempool sync command source file
 */

#include "synchronize.h"

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
#include "libpmempool.h"

/*
 * pmempool_sync_context -- context and arguments for sync command
 */
struct pmempool_sync_context {
	unsigned flags;		/* flags which modify the command execution */
	char *poolset_file;	/* a path to a poolset file */
};

/*
 * pmempool_sync_default -- default arguments for sync command
 */
static const struct pmempool_sync_context pmempool_sync_default = {
	.flags		= 0,
	.poolset_file	= NULL,
};

/*
 * help_str -- string for help message
 */
static const char *help_str =
"Check consistency of a pool\n"
"\n"
"Common options:\n"
"  -d, --dry-run        do not apply changes, only check for viability of"
" synchronization\n"
"  -v, --verbose        increase verbosity level\n"
"  -h, --help           display this help and exit\n"
"\n"
"For complete documentation see %s-sync(1) manual page.\n"
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
 * print_usage -- (internal) print application usage short description
 */
static void
print_usage(char *appname)
{
	printf("usage: %s sync [<options>] <poolset_file>\n", appname);
}

/*
 * print_version -- (internal) print version string
 */
static void
print_version(char *appname)
{
	printf("%s %s\n", appname, SRCVERSION);
}

/*
 * pmempool_sync_help -- print help message for the sync command
 */
void
pmempool_sync_help(char *appname)
{
	print_usage(appname);
	print_version(appname);
	printf(help_str, appname);
}

/*
 * pmempool_sync_parse_args -- (internal) parse command line arguments
 */
static int
pmempool_sync_parse_args(struct pmempool_sync_context *ctx, char *appname,
		int argc, char *argv[])
{
	int opt;
	while ((opt = getopt_long(argc, argv, "dhv",
			long_options, NULL)) != -1) {
		switch (opt) {
		case 'd':
			ctx->flags = PMEMPOOL_DRY_RUN;
			break;
		case 'h':
			pmempool_sync_help(appname);
			exit(EXIT_SUCCESS);
		case 'v':
			out_set_vlevel(1);
			break;
		default:
			print_usage(appname);
			exit(EXIT_FAILURE);
		}
	}

	if (optind < argc) {
		ctx->poolset_file = argv[optind];
	} else {
		print_usage(appname);
		exit(EXIT_FAILURE);
	}

	return 0;
}

/*
 * pmempool_sync_func -- main function for the sync command
 */
int
pmempool_sync_func(char *appname, int argc, char *argv[])
{
	int ret = 0;
	struct pmempool_sync_context ctx = pmempool_sync_default;

	/* parse command line arguments */
	if ((ret = pmempool_sync_parse_args(&ctx, appname, argc, argv)))
		return ret;

	ret = pmempool_sync(ctx.poolset_file, ctx.flags);

	if (ret) {
		outv_err("failed to synchronize: %s\n", pmempool_errormsg());
		if (errno)
			outv_err("%s\n", strerror(errno));
		return -1;
	} else {
		outv(1, "%s: synchronized\n", ctx.poolset_file);
		return 0;
	}
}

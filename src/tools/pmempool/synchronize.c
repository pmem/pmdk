// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016-2018, Intel Corporation */

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
static const char * const help_str =
"Check consistency of a pool\n"
"\n"
"Common options:\n"
"  -b, --bad-blocks     fix bad blocks - it requires creating or reading special recovery files\n"
"  -d, --dry-run        do not apply changes, only check for viability of synchronization\n"
"  -v, --verbose        increase verbosity level\n"
"  -h, --help           display this help and exit\n"
"\n"
"For complete documentation see %s-sync(1) manual page.\n"
;

/*
 * long_options -- command line options
 */
static const struct option long_options[] = {
	{"bad-blocks",	no_argument,		NULL,	'b'},
	{"dry-run",	no_argument,		NULL,	'd'},
	{"help",	no_argument,		NULL,	'h'},
	{"verbose",	no_argument,		NULL,	'v'},
	{NULL,		0,			NULL,	 0 },
};

/*
 * print_usage -- (internal) print application usage short description
 */
static void
print_usage(const char *appname)
{
	printf("usage: %s sync [<options>] <poolset_file>\n", appname);
}

/*
 * print_version -- (internal) print version string
 */
static void
print_version(const char *appname)
{
	printf("%s %s\n", appname, SRCVERSION);
}

/*
 * pmempool_sync_help -- print help message for the sync command
 */
void
pmempool_sync_help(const char *appname)
{
	print_usage(appname);
	print_version(appname);
	printf(help_str, appname);
}

/*
 * pmempool_sync_parse_args -- (internal) parse command line arguments
 */
static int
pmempool_sync_parse_args(struct pmempool_sync_context *ctx, const char *appname,
		int argc, char *argv[])
{
	int opt;
	while ((opt = getopt_long(argc, argv, "bdhv",
			long_options, NULL)) != -1) {
		switch (opt) {
		case 'd':
			ctx->flags |= PMEMPOOL_SYNC_DRY_RUN;
			break;
		case 'b':
			ctx->flags |= PMEMPOOL_SYNC_FIX_BAD_BLOCKS;
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
pmempool_sync_func(const char *appname, int argc, char *argv[])
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

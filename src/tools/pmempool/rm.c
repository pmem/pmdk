// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2014-2022, Intel Corporation */

/*
 * rm.c -- pmempool rm command main source file
 */

#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <err.h>
#include <stdio.h>
#include <fcntl.h>

#include "os.h"
#include "out.h"
#include "common.h"
#include "output.h"
#include "file.h"
#include "rm.h"
#include "set.h"

enum ask_type {
	ASK_SOMETIMES,	/* ask before removing write-protected files */
	ASK_ALWAYS,	/* always ask */
	ASK_NEVER,	/* never ask */
};

/* verbosity level */
static int vlevel;
/* force remove and ignore errors */
static int force;
/* poolset files options */
#define RM_POOLSET_NONE		(0)
#define RM_POOLSET_LOCAL	(1 << 0)
#define RM_POOLSET_ALL		(RM_POOLSET_LOCAL)
static int rm_poolset_mode;
/* mode of interaction */
static enum ask_type ask_mode;

/* help message */
static const char * const help_str =
"Remove pool file or all files from poolset\n"
"\n"
"Available options:\n"
"  -h, --help           Print this help message.\n"
"  -v, --verbose        Be verbose.\n"
"  -s, --only-pools     Remove only pool files (default).\n"
"  -a, --all            Remove all poolset files.\n"
"  -l, --local          Remove local poolset files\n"
"  -f, --force          Ignore nonexisting files.\n"
"  -i, --interactive    Prompt before every single removal.\n"
"\n"
"For complete documentation see %s-rm(1) manual page.\n";

/* short options string */
static const char *optstr = "hvsfial";
/* long options */
static const struct option long_options[] = {
	{"help",	no_argument,		NULL, 'h'},
	{"verbose",	no_argument,		NULL, 'v'},
	{"only-pools",	no_argument,		NULL, 's'},
	{"all",		no_argument,		NULL, 'a'},
	{"local",	no_argument,		NULL, 'l'},
	{"force",	no_argument,		NULL, 'f'},
	{"interactive",	no_argument,		NULL, 'i'},
	{NULL,		0,			NULL,  0 },
};

/*
 * print_usage -- print usage message
 */
static void
print_usage(const char *appname)
{
	printf("Usage: %s rm [<args>] <files>\n", appname);
}

/*
 * pmempool_rm_help -- print help message
 */
void
pmempool_rm_help(const char *appname)
{
	print_usage(appname);
	printf(help_str, appname);
}

/*
 * rm_file -- remove single file
 */
static int
rm_file(const char *file)
{
	int write_protected = os_access(file, W_OK) != 0;
	char cask = 'y';
	switch (ask_mode) {
	case ASK_ALWAYS:
		cask = '?';
		break;
	case ASK_NEVER:
		cask = 'y';
		break;
	case ASK_SOMETIMES:
		cask = write_protected ? '?' : 'y';
		break;
	default:
		outv_err("unknown state");
		return 1;
	}

	const char *pre_msg = write_protected ? "write-protected " : "";
	char ans = ask_Yn(cask, "remove %sfile '%s' ?", pre_msg, file);
	if (ans == 'y') {
		if (util_unlink(file)) {
			outv_err("cannot remove file '%s'", file);
			return 1;
		}

		outv(1, "removed '%s'\n", file);
	}

	return 0;
}

/*
 * rm_poolset_cb -- (internal) callback for removing replicas
 */
static int
rm_poolset_cb(struct part_file *pf, void *arg)
{
	int *error = (int *)arg;
	int ret;

	const char *part_file = pf->part->path;

	outv(2, "part file   : %s\n", part_file);

	int exists = util_file_exists(part_file);
	if (exists < 0)
		ret = 1;
	else if (!exists) {
		/*
		 * Ignore not accessible file if force
		 * flag is set.
		 */
		if (force)
			return 0;

		ret = 1;
		outv_err("!cannot remove file '%s'", part_file);
	} else {
		ret = rm_file(part_file);
	}

	if (ret)
		*error = ret;

	return 0;
}

/*
 * rm_poolset -- remove files parsed from poolset file
 */
static int
rm_poolset(const char *file)
{
	int error = 0;
	int ret = util_poolset_foreach_part(file, rm_poolset_cb, &error);
	if (ret == -1) {
		outv_err("parsing poolset failed: %s\n",
				out_get_errormsg());
		return ret;
	}

	if (error && !force) {
		outv_err("!removing '%s' failed\n", file);
		return error;
	}

	return 0;
}

/*
 * pmempool_rm_func -- main function for rm command
 */
int
pmempool_rm_func(const char *appname, int argc, char *argv[])
{
	/* by default do not remove any poolset files */
	rm_poolset_mode = RM_POOLSET_NONE;

	int opt;
	while ((opt = getopt_long(argc, argv, optstr,
			long_options, NULL)) != -1) {
		switch (opt) {
		case 'h':
			pmempool_rm_help(appname);
			return 0;
		case 'v':
			vlevel++;
			break;
		case 's':
			rm_poolset_mode = RM_POOLSET_NONE;
			break;
		case 'a':
			rm_poolset_mode |= RM_POOLSET_ALL;
			break;
		case 'l':
			rm_poolset_mode |= RM_POOLSET_LOCAL;
			break;
		case 'f':
			force = 1;
			ask_mode = ASK_NEVER;
			break;
		case 'i':
			ask_mode = ASK_ALWAYS;
			break;
		default:
			print_usage(appname);
			return 1;
		}
	}

	out_set_vlevel(vlevel);

	if (optind == argc) {
		print_usage(appname);
		return 1;
	}

	int lret = 0;
	for (int i = optind; i < argc; i++) {
		char *file = argv[i];
		/* check if file exists and we can read it */
		int exists = os_access(file, F_OK | R_OK) == 0;
		if (!exists) {
			/* ignore not accessible file if force flag is set */
			if (force)
				continue;

			outv_err("!cannot remove '%s'", file);
			lret = 1;
			continue;
		}

		int is_poolset = util_is_poolset_file(file);
		if (is_poolset < 0) {
			outv(1, "%s: cannot determine type of file", file);
			if (force)
				continue;
		}

		if (is_poolset)
			outv(2, "poolset file: %s\n", file);
		else
			outv(2, "pool file   : %s\n", file);

		int ret;
		if (is_poolset) {
			ret = rm_poolset(file);
			if (!ret && (rm_poolset_mode & RM_POOLSET_LOCAL))
				ret = rm_file(file);
		} else {
			ret = rm_file(file);
		}

		if (ret)
			lret = ret;
	}

	return lret;
}

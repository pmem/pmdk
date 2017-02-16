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
 * rm.c -- pmempool rm command main source file
 */

#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <err.h>
#include <stdio.h>
#include <fcntl.h>

#include "out.h"
#include "common.h"
#include "output.h"
#include "file.h"
#include "rm.h"
#include "set.h"

#ifdef USE_RPMEM
#include "librpmem.h"
#endif

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
#define RM_POOLSET_REMOTE	(1 << 1)
#define RM_POOLSET_ALL		(RM_POOLSET_LOCAL | RM_POOLSET_REMOTE)
static int rm_poolset_mode;
/* mode of interaction */
static enum ask_type ask_mode;
/* indicates whether librpmem is available */
static int rpmem_avail;

/* help message */
static const char *help_str =
"Remove pool file or all files from poolset\n"
"\n"
"Available options:\n"
"  -h, --help           Print this help message.\n"
"  -v, --verbose        Be verbose.\n"
"  -s, --only-pools     Remove only pool files (default).\n"
"  -p, --pool-set       Remove all poolset files - local and remote.\n"
"  -l, --local-set      Remove local poolset files\n"
"  -r, --remote-set     Remove remote poolset files\n"
"  -f, --force          Ignore nonexisting files.\n"
"  -i, --interactive    Prompt before every single removal.\n"
"\n"
"For complete documentation see %s-rm(1) manual page.\n";

/* short options string */
static const char *optstr = "hvsfialr";
/* long options */
static const struct option long_options[] = {
	{"help",	no_argument,		NULL, 'h'},
	{"verbose",	no_argument,		NULL, 'v'},
	{"only-pools",	no_argument,		NULL, 's'},
	{"all",		no_argument,		NULL, 'a'},
	{"local",	no_argument,		NULL, 'l'},
	{"remote",	no_argument,		NULL, 'r'},
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
pmempool_rm_help(char *appname)
{
	print_usage(appname);
	printf(help_str, appname);
}

/*
 * rm_file -- remove single file
 */
static void
rm_file(const char *file)
{
	int write_protected = access(file, W_OK) != 0;
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
	}

	const char *pre_msg = write_protected ? "write-protected " : "";
	if (ask_Yn(cask, "remove %sfile '%s' ?", pre_msg, file) == 'y') {
		if (util_unlink(file))
			err(1, "cannot remove file '%s'", file);
		outv(1, "removed '%s'\n", file);
	}
}

/*
 * remove_remote -- (internal) remove remote pool
 */
static int
remove_remote(const char *target, const char *pool_set)
{
#ifdef USE_RPMEM
	char cask = 'y';
	switch (ask_mode) {
	case ASK_ALWAYS:
		cask = '?';
		break;
	case ASK_NEVER:
	case ASK_SOMETIMES:
		cask = 'y';
		break;
	}

	if (ask_Yn(cask, "remove remote pool '%s' on '%s'?",
		pool_set, target) != 'y')
		return 0;

	if (!rpmem_avail) {
		if (force) {
			outv(1, "cannot remove '%s' on '%s' -- "
				"librpmem not available", pool_set, target);
			return 0;
		}

		outv_err("cannot remove '%s' on '%s' -- "
			"librpmem not available", pool_set, target);
		return 1;
	}

	int flags = 0;
	if (rm_poolset_mode & RM_POOLSET_REMOTE)
		flags |= RPMEM_REMOVE_POOL_SET;
	if (force)
		flags |= RPMEM_REMOVE_FORCE;

	int ret = Rpmem_remove(target, pool_set, flags);
	if (ret) {
		if (force) {
			ret = 0;
			outv(1, "cannot remove '%s' on '%s'",
					pool_set, target);
		} else {
			/*
			 * Callback cannot return < 0 value because it
			 * is interpretted as error in parsing poolset file.
			 */
			ret = 1;
			outv_err("cannot remove '%s' on '%s'",
					pool_set, target);
		}
	} else {
		outv(1, "removed '%s' on '%s'\n",
				pool_set, target);
	}

	return ret;
#else
	outv_err("remote replication not supported");
	return 1;
#endif
}

/*
 * rm_poolset_cb -- (internal) callback for removing replicas
 */
static int
rm_poolset_cb(struct part_file *pf, void *arg)
{
	if (pf->is_remote) {
		return remove_remote(pf->node_addr, pf->pool_desc);
	} else {
		const char *part_file = pf->path;

		outv(2, "part file   : %s\n", part_file);

		int exists = access(part_file, F_OK) == 0;
		if (!exists) {
			/*
			 * Ignore not accessible file if force
			 * flag is set.
			 */
			if (force)
				return 0;

			err(1, "cannot remove file '%s'", part_file);
		}

		rm_file(part_file);
	}

	return 0;
}

/*
 * rm_poolset -- remove files parsed from poolset file
 */
static void
rm_poolset(const char *file)
{
	int ret = util_poolset_foreach_part(file, rm_poolset_cb, NULL);
	if (ret) {
		if (ret == -1) {
			outv_err("parsing poolset failed: %s\n",
					out_get_errormsg());
			exit(1);
		}
		if (!force) {
			outv_err("removing '%s' failed\n", file);
			exit(1);
		}
		return;
	}
}

/*
 * pmempool_rm_func -- main function for rm command
 */
int
pmempool_rm_func(char *appname, int argc, char *argv[])
{
#ifdef USE_RPMEM
	util_remote_init();
	/*
	 * Try to load librpmem, if loading failed -
	 * assume it is not available.
	 */
	rpmem_avail = !util_remote_load();
#endif

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
		case 'r':
			rm_poolset_mode |= RM_POOLSET_REMOTE;
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
			return -1;
		}
	}

	out_set_vlevel(vlevel);

	if (optind == argc) {
		print_usage(appname);
		return -1;
	}

	for (int i = optind; i < argc; i++) {
		char *file = argv[i];
		/* check if file exists and we can read it */
		int exists = access(file, F_OK | R_OK) == 0;
		if (!exists) {
			/* ignore not accessible file if force flag is set */
			if (force)
				continue;
			err(1, "cannot remove '%s'", file);
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

		if (is_poolset) {
			rm_poolset(file);
			if (rm_poolset_mode & RM_POOLSET_LOCAL)
				rm_file(file);
		} else {
			rm_file(file);
		}
	}

#ifdef USE_RPMEM
	util_remote_fini();
#endif

	return 0;
}

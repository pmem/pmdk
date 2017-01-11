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
 * check.c -- pmempool check command source file
 */
#include <getopt.h>
#include <stdlib.h>

#include "common.h"
#include "check.h"
#include "output.h"
#include "set.h"
#include "file.h"

#include "libpmempool.h"

typedef enum
{
	CHECK_RESULT_CONSISTENT,
	CHECK_RESULT_NOT_CONSISTENT,
	CHECK_RESULT_REPAIRED,
	CHECK_RESULT_CANNOT_REPAIR,
	CHECK_RESULT_ERROR
} check_result_t;

/*
 * pmempool_check_context -- context and arguments for check command
 */
struct pmempool_check_context {
	int verbose;		/* verbosity level */
	char *fname;		/* file name */
	struct pool_set_file *pfile;
	bool repair;		/* do repair */
	bool backup;		/* do backup */
	bool advanced;		/* do advanced repairs */
	char *backup_fname;	/* backup file name */
	bool exec;		/* do execute */
	char ans;		/* default answer on all questions or '?' */
};

/*
 * pmempool_check_default -- default arguments for check command
 */
static const struct pmempool_check_context pmempool_check_default = {
	.verbose	= 1,
	.fname		= NULL,
	.repair		= false,
	.backup		= false,
	.backup_fname	= NULL,
	.advanced	= false,
	.exec		= true,
	.ans		= '?',
};

/*
 * help_str -- string for help message
 */
static const char *help_str =
"Check consistency of a pool\n"
"\n"
"Common options:\n"
"  -r, --repair         try to repair a pool file if possible\n"
"  -y, --yes            answer yes to all questions\n"
"  -N, --no-exec        don't execute, just show what would be done\n"
"  -b, --backup <file>  create backup of a pool file before executing\n"
"  -a, --advanced       perform advanced repairs\n"
"  -q, --quiet          be quiet and don't print any messages\n"
"  -v, --verbose        increase verbosity level\n"
"  -h, --help           display this help and exit\n"
"\n"
"For complete documentation see %s-check(1) manual page.\n"
;

/*
 * long_options -- command line options
 */
static const struct option long_options[] = {
	{"repair",	no_argument,		0,	'r'},
	{"yes",		no_argument,		0,	'y'},
	{"no-exec",	no_argument,		0,	'N'},
	{"backup",	required_argument,	0,	'b'},
	{"advanced",	no_argument,		0,	'a'},
	{"quiet",	no_argument,		0,	'q'},
	{"verbose",	no_argument,		0,	'v'},
	{"help",	no_argument,		0,	'h'},
	{0,		0,			0,	 0 },
};

/*
 * print_usage -- print short description of application's usage
 */
static void
print_usage(char *appname)
{
	printf("Usage: %s check [<args>] <file>\n", appname);
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
 * pmempool_check_help -- print help message for check command
 */
void
pmempool_check_help(char *appname)
{
	print_usage(appname);
	print_version(appname);
	printf(help_str, appname);
}

/*
 * pmempool_check_parse_args -- parse command line arguments
 */
static int
pmempool_check_parse_args(struct pmempool_check_context *pcp, char *appname,
		int argc, char *argv[])
{
	int opt;
	while ((opt = getopt_long(argc, argv, "ahvrNb:qy",
			long_options, NULL)) != -1) {
		switch (opt) {
		case 'r':
			pcp->repair = true;
			break;
		case 'y':
			pcp->ans = 'y';
			break;
		case 'N':
			pcp->exec = false;
			break;
		case 'b':
			pcp->backup = true;
			pcp->backup_fname = optarg;
			break;
		case 'a':
			pcp->advanced = true;
			break;
		case 'q':
			pcp->verbose = 0;
			break;
		case 'v':
			pcp->verbose = 2;
			break;
		case 'h':
			pmempool_check_help(appname);
			exit(EXIT_SUCCESS);
		default:
			print_usage(appname);
			exit(EXIT_FAILURE);
		}
	}

	if (optind < argc) {
		pcp->fname = argv[optind];
	} else {
		print_usage(appname);
		exit(EXIT_FAILURE);
	}

	if (!pcp->repair && !pcp->exec) {
		outv_err("'-N' option requires '-r'\n");
		exit(EXIT_FAILURE);
	}

	if (!pcp->repair && pcp->backup) {
		outv_err("'-b' option requires '-r'\n");
		exit(EXIT_FAILURE);
	}

	return 0;
}

static check_result_t pmempool_check_2_check_res_t[] =
{
	[PMEMPOOL_CHECK_RESULT_CONSISTENT] = CHECK_RESULT_CONSISTENT,
	[PMEMPOOL_CHECK_RESULT_NOT_CONSISTENT] = CHECK_RESULT_NOT_CONSISTENT,
	[PMEMPOOL_CHECK_RESULT_REPAIRED] = CHECK_RESULT_REPAIRED,
	[PMEMPOOL_CHECK_RESULT_CANNOT_REPAIR] = CHECK_RESULT_CANNOT_REPAIR,
	[PMEMPOOL_CHECK_RESULT_ERROR] = CHECK_RESULT_ERROR,
};

static char *
check_ask(const char *msg)
{
	char answer = ask_Yn('?', msg);

	switch (answer) {
	case 'y':
		return "yes";
	case 'n':
		return "no";
	default:
		return "?";
	}
}

static check_result_t
pmempool_check_perform(struct pmempool_check_context *pc)
{
	struct pmempool_check_args args = {
		.path	= pc->fname,
		.backup_path	= pc->backup_fname,
		.pool_type	= PMEMPOOL_POOL_TYPE_DETECT,
		.flags		= PMEMPOOL_CHECK_FORMAT_STR
	};

	if (pc->repair)
		args.flags |= PMEMPOOL_CHECK_REPAIR;
	if (!pc->exec)
		args.flags |= PMEMPOOL_CHECK_DRY_RUN;
	if (pc->advanced)
		args.flags |= PMEMPOOL_CHECK_ADVANCED;
	if (pc->ans == 'y')
		args.flags |= PMEMPOOL_CHECK_ALWAYS_YES;
	if (pc->verbose == 2)
		args.flags |= PMEMPOOL_CHECK_VERBOSE;

	PMEMpoolcheck *ppc = pmempool_check_init(&args, sizeof(args));

	if (ppc == NULL)
		return CHECK_RESULT_ERROR;

	struct pmempool_check_status *status = NULL;
	while ((status = pmempool_check(ppc)) != NULL) {
		switch (status->type) {
		case PMEMPOOL_CHECK_MSG_TYPE_ERROR:
			outv(1, "%s\n", status->str.msg);
			break;
		case PMEMPOOL_CHECK_MSG_TYPE_INFO:
			outv(2, "%s\n", status->str.msg);
			break;
		case PMEMPOOL_CHECK_MSG_TYPE_QUESTION:
			status->str.answer = check_ask(status->str.msg);
			break;
		default:
			pmempool_check_end(ppc);
			exit(EXIT_FAILURE);
		}
	}

	enum pmempool_check_result ret = pmempool_check_end(ppc);

	return pmempool_check_2_check_res_t[ret];
}

/*
 * pmempool_check_func -- main function for check command
 */
int
pmempool_check_func(char *appname, int argc, char *argv[])
{
	int ret = 0;
	check_result_t res = CHECK_RESULT_CONSISTENT;
	struct pmempool_check_context pc = pmempool_check_default;

	/* parse command line arguments */
	ret = pmempool_check_parse_args(&pc, appname, argc, argv);
	if (ret)
		return ret;

	/* set verbosity level */
	out_set_vlevel(pc.verbose);

	res = pmempool_check_perform(&pc);

	switch (res) {
	case CHECK_RESULT_CONSISTENT:
		outv(2, "%s: consistent\n", pc.fname);
		ret = 0;
		break;
	case CHECK_RESULT_NOT_CONSISTENT:
		outv(1, "%s: not consistent\n", pc.fname);
		ret = -1;
		break;
	case CHECK_RESULT_REPAIRED:
		outv(1, "%s: repaired\n", pc.fname);
		ret = 0;
		break;
	case CHECK_RESULT_CANNOT_REPAIR:
		outv(1, "%s: cannot repair\n", pc.fname);
		ret = -1;
		break;
	case CHECK_RESULT_ERROR:
		if (errno)
			outv_err("%s\n", strerror(errno));
		if (pc.repair)
			outv_err("repairing failed\n");
		else
			outv_err("checking consistency failed\n");
		ret = -1;
		break;
	}

	return ret;
}

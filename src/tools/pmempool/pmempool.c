// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2014-2022, Intel Corporation */

/*
 * pmempool.c -- pmempool main source file
 */

#include <stdio.h>
#include <libgen.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#include <stdbool.h>
#include "common.h"
#include "output.h"
#include "info.h"
#include "create.h"
#include "dump.h"
#include "check.h"
#include "rm.h"
#include "convert.h"
#include "synchronize.h"
#include "transform.h"
#include "feature.h"
#include "set.h"
#include "pmemcommon.h"

#define APPNAME	"pmempool"

#define PMEMPOOL_TOOL_LOG_PREFIX "pmempool"
#define PMEMPOOL_TOOL_LOG_LEVEL_VAR "PMEMPOOL_TOOL_LOG_LEVEL"
#define PMEMPOOL_TOOL_LOG_FILE_VAR "PMEMPOOL_TOOL_LOG_FILE"

/*
 * command -- struct for pmempool commands definition
 */
struct command {
	const char *name;
	const char *brief;
	int (*func)(const char *, int, char *[]);
	void (*help)(const char *);
};

static const struct command *get_command(const char *cmd_str);
static void print_help(const char *appname);

/*
 * long_options -- pmempool command line arguments
 */
static const struct option long_options[] = {
	{"version",	no_argument,	NULL,	'V'},
	{"help",	no_argument,	NULL,	'h'},
	{NULL,		0,		NULL,	 0 },
};

/*
 * help_help -- prints help message for help command
 */
static void
help_help(const char *appname)
{
	printf("Usage: %s help <command>\n", appname);
}

/*
 * help_func -- prints help message for specified command
 */
static int
help_func(const char *appname, int argc, char *argv[])
{
	if (argc > 1) {
		char *cmd_str = argv[1];
		const struct command *cmdp = get_command(cmd_str);

		if (cmdp && cmdp->help) {
			cmdp->help(appname);
			return 0;
		} else {
			outv_err("No help text for '%s' command\n", cmd_str);
			return -1;
		}
	} else {
		print_help(appname);
		return -1;
	}
}

/*
 * commands -- definition of all pmempool commands
 */
static const struct command commands[] = {
	{
		.name = "info",
		.brief = "print information and statistics about a pool",
		.func = pmempool_info_func,
		.help = pmempool_info_help,
	},
	{
		.name = "create",
		.brief = "create a pool",
		.func = pmempool_create_func,
		.help = pmempool_create_help,
	},
	{
		.name = "dump",
		.brief = "dump user data from a pool",
		.func = pmempool_dump_func,
		.help = pmempool_dump_help,
	},
	{
		.name = "check",
		.brief = "check consistency of a pool",
		.func = pmempool_check_func,
		.help = pmempool_check_help,
	},
	{
		.name = "rm",
		.brief = "remove pool or poolset",
		.func = pmempool_rm_func,
		.help = pmempool_rm_help,
	},
	{
		.name = "convert",
		.brief = "perform pool layout conversion",
		.func = pmempool_convert_func,
		.help = pmempool_convert_help,
	},
	{
		.name = "sync",
		.brief = "synchronize data between replicas",
		.func = pmempool_sync_func,
		.help = pmempool_sync_help,
	},
	{
		.name = "transform",
		.brief = "modify internal structure of a poolset",
		.func = pmempool_transform_func,
		.help = pmempool_transform_help,
	},
	{
		.name = "feature",
		.brief = "toggle / query pool features",
		.func = pmempool_feature_func,
		.help = pmempool_feature_help,
	},
	{
		.name = "help",
		.brief = "print help text about a command",
		.func = help_func,
		.help = help_help,
	},
};

/*
 * number of pmempool commands
 */
#define COMMANDS_NUMBER	(sizeof(commands) / sizeof(commands[0]))

/*
 * print_version -- prints pmempool version message
 */
static void
print_version(const char *appname)
{
	printf("%s %s\n", appname, SRCVERSION);
}

/*
 * print_usage -- prints pmempool usage message
 */
static void
print_usage(const char *appname)
{
	printf("usage: %s [--version] [--help] <command> [<args>]\n", appname);
}

/*
 * print_help -- prints pmempool help message
 */
static void
print_help(const char *appname)
{
	print_usage(appname);
	print_version(appname);
	printf("\n");
	printf("Options:\n");
	printf("  -V, --version        display version\n");
	printf("  -h, --help           display this help and exit\n");
	printf("\n");
	printf("The available commands are:\n");
	unsigned i;
	for (i = 0; i < COMMANDS_NUMBER; i++) {
		const char *format = (strlen(commands[i].name) / 8)
				? "%s\t- %s\n" : "%s\t\t- %s\n";
		printf(format, commands[i].name, commands[i].brief);
	}
	printf("\n");
	printf("For complete documentation see %s(1) manual page.\n", appname);
}

/*
 * get_command -- returns command for specified command name
 */
static const struct command *
get_command(const char *cmd_str)
{
	unsigned i;
	for (i = 0; i < COMMANDS_NUMBER; i++) {
		if (strcmp(cmd_str, commands[i].name) == 0)
			return &commands[i];
	}

	return NULL;
}

int
main(int argc, char *argv[])
{
	int opt;
	int option_index;
	int ret = 0;
#ifdef _WIN32
	util_suppress_errmsg();
	wchar_t **wargv = CommandLineToArgvW(GetCommandLineW(), &argc);
	for (int i = 0; i < argc; i++) {
		argv[i] = util_toUTF8(wargv[i]);
		if (argv[i] == NULL) {
			for (i--; i >= 0; i--)
				free(argv[i]);
			outv_err("Error during arguments conversion\n");
			return 1;
		}
	}
#endif

	common_init(PMEMPOOL_TOOL_LOG_PREFIX,
			PMEMPOOL_TOOL_LOG_LEVEL_VAR,
			PMEMPOOL_TOOL_LOG_FILE_VAR,
			0 /* major version */,
			0 /* minor version */);

	if (argc < 2) {
		print_usage(APPNAME);
		goto end;
	}

	while ((opt = getopt_long(2, argv, "Vh",
			long_options, &option_index)) != -1) {
		switch (opt) {
		case 'V':
			print_version(APPNAME);
			goto end;
		case 'h':
			print_help(APPNAME);
			goto end;
		default:
			print_usage(APPNAME);
			ret = 1;
			goto end;
		}
	}

	char *cmd_str = argv[optind];

	const struct command *cmdp = get_command(cmd_str);

	if (cmdp) {
		ret = cmdp->func(APPNAME, argc - 1, argv + 1);
	} else {
		outv_err("'%s' -- unknown command\n", cmd_str);
		ret = 1;
	}

end:

	common_fini();

#ifdef _WIN32
	for (int i = argc; i > 0; i--)
		free(argv[i - 1]);
#endif
	if (ret)
		return 1;

	return 0;
}

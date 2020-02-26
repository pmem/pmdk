// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016-2017, Intel Corporation */
/*
 * Copyright 2016, FUJITSU TECHNOLOGY SOLUTIONS GMBH
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
 * ===========================================================================
 *
 *       Filename:  arttree_structures.c
 *
 *    Description:  Examine pmem structures; structures and unions taken from
 *                  the preprocessor output of a libpmemobj compatible program.
 *
 *         Author:  Andreas Bluemle, Dieter Kasper
 *                  Andreas.Bluemle.external@ts.fujitsu.com
 *                  dieter.kasper@ts.fujitsu.com
 *
 *   Organization:  FUJITSU TECHNOLOGY SOLUTIONS GMBH
 *
 * ===========================================================================
 */

#ifdef __FreeBSD__
#define _WITH_GETLINE
#endif
#include <stdio.h>
#include <fcntl.h>
#include <libgen.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include "arttree_structures.h"

#include <stdarg.h>

#define APPNAME "examine_arttree"
#define SRCVERSION "0.2"

size_t art_node_sizes[art_node_types] = {
	sizeof(art_node4),
	sizeof(art_node16),
	sizeof(art_node48),
	sizeof(art_node256),
	sizeof(art_leaf),
	sizeof(art_node_u),
	sizeof(art_node),
	sizeof(art_tree_root),
	sizeof(var_string),
};

char *art_node_names[art_node_types] = {
	"art_node4",
	"art_node16",
	"art_node48",
	"art_node256",
	"art_leaf",
	"art_node_u",
	"art_node",
	"art_tree_root",
	"var_string"
};

/*
 * long_options -- command line arguments
 */
static const struct option long_options[] = {
	{"help",	no_argument,	NULL,	'h'},
	{NULL,		0,		NULL,	 0 },
};

/*
 * command -- struct for commands definition
 */
struct command {
	const char *name;
	const char *brief;
	int (*func)(char *, struct pmem_context *, int, char *[]);
	void (*help)(char *);
};

/*
 * number of arttree_structures commands
 */
#define COMMANDS_NUMBER (sizeof(commands) / sizeof(commands[0]))

static void print_help(char *appname);
static void print_usage(char *appname);
static void print_version(char *appname);
static int quit_func(char *appname, struct pmem_context *ctx,
		int argc, char *argv[]);
static void quit_help(char *appname);
static int set_root_func(char *appname, struct pmem_context *ctx,
		int argc, char *argv[]);
static void set_root_help(char *appname);
static int help_func(char *appname, struct pmem_context *ctx,
		int argc, char *argv[]);
static void help_help(char *appname);
static struct command *get_command(char *cmd_str);
static int ctx_init(struct pmem_context *ctx, char *filename);

static int arttree_structures_func(char *appname, struct pmem_context *ctx,
	int ac, char *av[]);
static void arttree_structures_help(char *appname);

static int arttree_info_func(char *appname, struct pmem_context *ctx,
	int ac, char *av[]);
static void arttree_info_help(char *appname);

extern int arttree_examine_func();
extern void arttree_examine_help();

extern int arttree_search_func();
extern void arttree_search_help();

void outv_err(const char *fmt, ...);
void outv_err_vargs(const char *fmt, va_list ap);

static struct command commands[] = {
	{
		.name = "structures",
		.brief = "print information about ART structures",
		.func = arttree_structures_func,
		.help = arttree_structures_help,
	},
	{
		.name = "info",
		.brief = "print information and statistics"
		    " about an ART tree pool",
		.func = arttree_info_func,
		.help = arttree_info_help,
	},
	{
		.name = "examine",
		.brief = "examine data structures from an ART tree",
		.func = arttree_examine_func,
		.help = arttree_examine_help,
	},
	{
		.name = "search",
		.brief = "search for a key in an ART tree",
		.func = arttree_search_func,
		.help = arttree_search_help,
	},
	{
		.name = "set_root",
		.brief = "define offset of root of an ART tree",
		.func = set_root_func,
		.help = set_root_help,
	},
	{
		.name = "help",
		.brief = "print help text about a command",
		.func = help_func,
		.help = help_help,
	},
	{
		.name = "quit",
		.brief = "quit ART tree structure examiner",
		.func = quit_func,
		.help = quit_help,
	},
};

static struct pmem_context ctx;

/*
 * outv_err -- print error message
 */
void
outv_err(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	outv_err_vargs(fmt, ap);
	va_end(ap);
}

/*
 * outv_err_vargs -- print error message
 */
void
outv_err_vargs(const char *fmt, va_list ap)
{
	fprintf(stderr, "error: ");
	vfprintf(stderr, fmt, ap);
	if (!strchr(fmt, '\n'))
		fprintf(stderr, "\n");
}

/*
 * print_usage -- prints usage message
 */
static void
print_usage(char *appname)
{
	printf("usage: %s [--help] <pmem file> <command> [<args>]\n", appname);
}

/*
 * print_version -- prints version message
 */
static void
print_version(char *appname)
{
	printf("%s %s\n", appname, SRCVERSION);
}

/*
 * print_help -- prints help message
 */
static void
print_help(char *appname)
{
	print_usage(appname);
	print_version(appname);
	printf("\n");
	printf("Options:\n");
	printf("  -h, --help           display this help and exit\n");
	printf("\n");
	printf("The available commands are:\n");
	for (size_t i = 0; i < COMMANDS_NUMBER; i++)
		printf("%s\t- %s\n", commands[i].name, commands[i].brief);
	printf("\n");
}

/*
 * set_root_help -- prints help message for set root command
 */
static void
set_root_help(char *appname)
{
	printf("Usage: set_root <offset>\n");
	printf("    define the offset of the art tree root\n");
}

/*
 * set_root_func -- set_root define the offset of the art tree root
 */
static int
set_root_func(char *appname, struct pmem_context *ctx, int argc, char *argv[])
{
	int retval = 0;
	uint64_t root_offset;

	if (argc == 2) {
		root_offset = strtol(argv[1], NULL, 0);
		ctx->art_tree_root_offset = root_offset;
	} else {
		set_root_help(appname);
		retval = 1;
	}
	return retval;
}

/*
 * quit_help -- prints help message for quit command
 */
static void
quit_help(char *appname)
{
	printf("Usage: quit\n");
	printf("    terminate arttree structure examiner\n");
}

/*
 * quit_func -- quit arttree structure examiner
 */
static int
quit_func(char *appname, struct pmem_context *ctx, int argc, char *argv[])
{
	printf("\n");
	exit(0);
	return 0;
}

/*
 * help_help -- prints help message for help command
 */
static void
help_help(char *appname)
{
	printf("Usage: %s help <command>\n", appname);
}

/*
 * help_func -- prints help message for specified command
 */
static int
help_func(char *appname, struct pmem_context *ctx, int argc, char *argv[])
{
	if (argc > 1) {
		char *cmd_str = argv[1];
		struct command *cmdp = get_command(cmd_str);

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

static const char *arttree_structures_help_str =
"Show information about known ART tree structures\n"
;

static void
arttree_structures_help(char *appname)
{
	printf("%s %s\n", appname, arttree_structures_help_str);
}

static int
arttree_structures_func(char *appname, struct pmem_context *ctx,
	int ac, char *av[])
{
	(void) appname;
	(void) ac;
	(void) av;

	printf(
	    "typedef struct pmemoid {\n"
	    " uint64_t pool_uuid_lo;\n"
	    " uint64_t off;\n"
	    "} PMEMoid;\n");
	printf("sizeof(PMEMoid) = %zu\n\n\n", sizeof(PMEMoid));

	printf(
	    "struct _art_node_u; typedef struct _art_node_u art_node_u;\n"
	    "struct _art_node_u { \n"
	    "    uint8_t art_node_type; \n"
	    "    uint8_t art_node_tag; \n"
	    "};\n");
	printf("sizeof(art_node_u) = %zu\n\n\n", sizeof(art_node_u));

	printf(
	    "struct _art_node; typedef struct _art_node art_node;\n"
	    "struct _art_node {\n"
	    "    uint8_t type;\n"
	    "    uint8_t num_children;\n"
	    "    uint32_t partial_len;\n"
	    "    unsigned char partial[10];\n"
	    "};\n");
	printf("sizeof(art_node) = %zu\n\n\n", sizeof(art_node));

	printf(
	    "typedef uint8_t _toid_art_node_toid_type_num[8];\n");
	printf("sizeof(_toid_art_node_toid_type_num[8]) = %zu\n\n\n",
	    sizeof(_toid_art_node_toid_type_num[8]));

	printf(
	    "union _toid_art_node_u_toid {\n"
	    "    PMEMoid oid;\n"
	    "    art_node_u *_type;\n"
	    "    _toid_art_node_u_toid_type_num *_type_num;\n"
	    "};\n");
	printf("sizeof(union _toid_art_node_u_toid) = %zu\n\n\n",
	    sizeof(union _toid_art_node_u_toid));

	printf(
	    "typedef uint8_t _toid_art_node_toid_type_num[8];\n");
	printf("sizeof(_toid_art_node_toid_type_num[8]) = %zu\n\n\n",
	    sizeof(_toid_art_node_toid_type_num[8]));

	printf(
	    "union _toid_art_node_toid {\n"
	    "    PMEMoid oid; \n"
	    "    art_node *_type; \n"
	    "    _toid_art_node_toid_type_num *_type_num;\n"
	    "};\n");
	printf("sizeof(union _toid_art_node_toid) = %zu\n\n\n",
	    sizeof(union _toid_art_node_toid));

	printf(
	    "struct _art_node4; typedef struct _art_node4 art_node4;\n"
	    "struct _art_node4 {\n"
	    "    art_node n;\n"
	    "    unsigned char keys[4];\n"
	    "    union _toid_art_node_u_toid children[4];\n"
	    "};\n");
	printf("sizeof(art_node4) = %zu\n\n\n", sizeof(art_node4));

	printf(
	    "struct _art_node16; typedef struct _art_node16 art_node16;\n"
	    "struct _art_node16 {\n"
	    "    art_node n;\n"
	    "    unsigned char keys[16];\n"
	    "    union _toid_art_node_u_toid children[16];\n"
	    "};\n");
	printf("sizeof(art_node16) = %zu\n\n\n", sizeof(art_node16));

	printf(
	    "struct _art_node48; typedef struct _art_node48 art_node48;\n"
	    "struct _art_node48 {\n"
	    "    art_node n;\n"
	    "    unsigned char keys[256];\n"
	    "    union _toid_art_node_u_toid children[48];\n"
	    "};\n");
	printf("sizeof(art_node48) = %zu\n\n\n", sizeof(art_node48));

	printf(
	    "struct _art_node256; typedef struct _art_node256 art_node256;\n"
	    "struct _art_node256 {\n"
	    "    art_ndoe n;\n"
	    "    union _toid_art_node_u_toid children[256];\n"
	    "};\n");
	printf("sizeof(art_node256) = %zu\n\n\n", sizeof(art_node256));

	printf(
	    "struct _art_leaf; typedef struct _art_leaf art_leaf;\n"
	    "struct _art_leaf {\n"
	    "    union _toid_var_string_toid value;\n"
	    "    union _toid_var_string_toid key;\n"
	    "};\n");
	printf("sizeof(art_leaf) = %zu\n\n\n", sizeof(art_leaf));

	return 0;
}

static const char *arttree_info_help_str =
"Show information about known ART tree structures\n"
;

static void
arttree_info_help(char *appname)
{
	printf("%s %s\n", appname, arttree_info_help_str);
}

static int
arttree_info_func(char *appname, struct pmem_context *ctx, int ac, char *av[])
{
	printf("%s: %s not yet implemented\n", appname, __FUNCTION__);

	return 0;
}

/*
 * get_command -- returns command for specified command name
 */
static struct command *
get_command(char *cmd_str)
{
	if (cmd_str == NULL) {
		return NULL;
	}

	for (size_t i = 0; i < COMMANDS_NUMBER; i++) {
		if (strcmp(cmd_str, commands[i].name) == 0)
			return &commands[i];
	}

	return NULL;
}

static int
ctx_init(struct pmem_context *ctx, char *filename)
{
	int errors = 0;

	if (filename == NULL)
		errors++;
	if (ctx == NULL)
		errors++;

	if (errors)
		return errors;

	ctx->filename = strdup(filename);
	assert(ctx->filename != NULL);
	ctx->fd = -1;
	ctx->addr = NULL;
	ctx->art_tree_root_offset = 0;

	if (access(ctx->filename, F_OK) != 0)
		return 1;

	if ((ctx->fd = open(ctx->filename, O_RDONLY)) == -1)
		return 1;

	struct stat stbuf;
	if (fstat(ctx->fd, &stbuf) < 0)
		return 1;
	ctx->psize = stbuf.st_size;

	if ((ctx->addr = mmap(NULL, ctx->psize, PROT_READ,
			MAP_SHARED, ctx->fd, 0)) == MAP_FAILED)
		return 1;

	return 0;
}

static void
ctx_fini(struct pmem_context *ctx)
{
	munmap(ctx->addr, ctx->psize);
	close(ctx->fd);
	free(ctx->filename);
}

int
main(int ac, char *av[])
{
	int opt;
	int option_index;
	int ret = 0;
	size_t len;
	ssize_t read;
	char *cmd_str;
	char *args[20];
	int nargs;
	char *line;
	struct command *cmdp = NULL;

	while ((opt = getopt_long(ac, av, "h",
			    long_options, &option_index)) != -1) {
		switch (opt) {
		case 'h':
			print_help(APPNAME);
			return 0;
		default:
			print_usage(APPNAME);
			return -1;
		}
	}

	if (optind >= ac) {
		fprintf(stderr, "ERROR: missing arguments\n");
		print_usage(APPNAME);
		return -1;
	}

	ctx_init(&ctx, av[optind]);

	if (optind + 1 < ac) {
		/* execute command as given on command line */
		cmd_str = av[optind + 1];
		cmdp = get_command(cmd_str);
		if (cmdp != NULL) {
			ret = cmdp->func(APPNAME, &ctx, ac - 2, av + 2);
		}
	} else {
		/* interactive mode: read commands and execute them */
		line = NULL;
		printf("\n> ");
		while ((read = getline(&line, &len, stdin)) != -1) {
			if (line[read - 1] == '\n') {
				line[read - 1] = '\0';
			}
			args[0] = strtok(line, " ");
			cmdp = get_command(args[0]);
			if (cmdp == NULL) {
				printf("[%s]: command not supported\n",
				    args[0] ? args[0] : "NULL");
				printf("\n> ");
				continue;
			}
			nargs = 1;
			while (1) {
				args[nargs] = strtok(NULL, " ");
				if (args[nargs] == NULL) {
					break;
				}
				nargs++;
			}
			ret = cmdp->func(APPNAME, &ctx, nargs, args);
			printf("\n> ");
		}
		if (line != NULL) {
			free(line);
		}
	}

	ctx_fini(&ctx);

	return ret;
}

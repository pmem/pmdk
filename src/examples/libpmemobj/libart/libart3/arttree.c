/*
 * Copyright 2017, FUJITSU TECHNOLOGY SOLUTIONS GMBH
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
 *     Filename:  arttree.c
 *
 *  Description:  arttree test program for ART trees
 *
 *       Author:  Andreas Bluemle, Dieter Kasper
 *                andreas.bluemle@itxperts.de
 *                dieter.kasper@ts.fujitsu.com
 *
 * Organization:  FUJITSU TECHNOLOGY SOLUTIONS GMBH
 *
 * ===========================================================================
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <stdbool.h>
#include <fcntl.h>
#include <emmintrin.h>
#include <stdarg.h>
#include "libpmemobj.h"
#include "obj.h"
#include "arttree.h"

#define APPNAME "arttree"
#define SRCVERSION "0.1"

/*
 * Macros to manipulate pointer tags
 */
#define IS_LEAF(x) (((uintptr_t)x & 1))
#define SET_LEAF(x) ((void *)((uintptr_t)x | 1))
#define LEAF_RAW(x) ((void *)((uintptr_t)x & ~1))

static int dump_art_node_callback(void *data,
	const unsigned char *key, uint32_t key_len,
	void *val, uint32_t val_len);
static int dump_art_leaf_callback(void *data,
	const unsigned char *key, uint32_t key_len,
	void *val, uint32_t val_len);
static int noop_art_tree_callback(void *data,
	const unsigned char *key, uint32_t key_len,
	void *val, uint32_t val_len);
static int iterate_leaf_callback(void *data,
	const unsigned char *key, uint32_t key_len,
	void *val, uint32_t val_len);
static void print_node_info(char *nodetype, uint64_t addr, const art_node *an);


struct datastore
{
	void *priv;
};

/*
 * context - main context of datastore
 */
struct ds_context
{
	char *filename;		/* name of pool file */
	int    mode;		/* operation mode */
	int    operations;	/* number of insert operations to perform */
	int    newpool;		/* complete new memory pool */
	art_tree *art_tree;	/* art_tree root */
	PMEMobjpool *pop;
	size_t psize;		/* size of pool */
	bool fileio;
	unsigned fmode;
	FILE   *input;
	FILE   *output;
	uint64_t address;
	unsigned char *key;		/* for SEARCH, INSERT and REMOVE */
	uint32_t key_len;
	unsigned char *value;	/* for INSERT */
	uint32_t val_len;
	int    type;
	int fd;			/* file descriptor for file io mode */
};

#define FILL 1<<1
#define INTERACTIVE 1<<3

struct ds_context my_context;

#define read_key(c, p) read_line(c, p)
#define read_value(c, p) read_line(c, p)

static void usage(char *progname);
int initialize_context(struct ds_context *ctx, int ac, char *av[]);
int add_elements(struct ds_context *ctx);
int lookup_elements(struct ds_context *ctx);
ssize_t read_line(struct ds_context *ctx, unsigned char **line);
void exit_handler(struct ds_context *ctx);
int art_tree_map_init(struct datastore *ds, struct ds_context *ctx);
void pmemobj_ds_set_priv(struct datastore *ds, void *priv);

static void print_help(char *appname);
static void print_version(char *appname);
static struct command *get_command(char *cmd_str);
static int help_func(char *appname, struct ds_context *ctx, int argc,
		char *argv[]);
static void help_help(char *appname);
static int quit_func(char *appname, struct ds_context *ctx, int argc,
		char *argv[]);
static void quit_help(char *appname);
static int set_output_func(char *appname, struct ds_context *ctx, int argc,
		char *argv[]);
static void set_output_help(char *appname);
static int arttree_fill_func(char *appname, struct ds_context *ctx,
		int ac, char *av[]);
static void arttree_fill_help(char *appname);
static int arttree_lookup_func(char *appname, struct ds_context *ctx,
		int ac, char *av[]);
static void arttree_lookup_help(char *appname);
static int arttree_search_func(char *appname, struct ds_context *ctx,
		int ac, char *av[]);
static void arttree_search_help(char *appname);
static int arttree_delete_func(char *appname, struct ds_context *ctx,
		int ac, char *av[]);
static void arttree_delete_help(char *appname);
static int arttree_dump_func(char *appname, struct ds_context *ctx,
		int ac, char *av[]);
static void arttree_dump_help(char *appname);
static int arttree_graph_func(char *appname, struct ds_context *ctx,
		int ac, char *av[]);
static void arttree_graph_help(char *appname);
static int arttree_iterate_func(char *appname, struct ds_context *ctx,
		int ac, char *av[]);
static void arttree_iterate_help(char *appname);
static int arttree_iterate_list_func(char *appname, struct ds_context *ctx,
		int ac, char *av[]);
static void arttree_iterate_list_help(char *appname);

static char *asciidump(unsigned char *s, int32_t len);

void outv_err(const char *fmt, ...);
void outv_err_vargs(const char *fmt, va_list ap);

/*
 * command -- struct for pmempool commands definition
 */
struct command {
	const char *name;
	const char *brief;
	int (*func)(char *, struct ds_context *, int, char *[]);
	void (*help)(char *);
};

struct command commands[] = {
	{
		.name = "fill",
		.brief = "create and fill an art tree",
		.func = arttree_fill_func,
		.help = arttree_fill_help,
	},
	{
		.name = "lookup",
		.brief = "lookup keys in an art tree",
		.func = arttree_lookup_func,
		.help = arttree_lookup_help,
	},
	{
		.name = "dump",
		.brief = "dump an art tree",
		.func = arttree_dump_func,
		.help = arttree_dump_help,
	},
	{
		.name = "graph",
		.brief = "dump an art tree for graphical conversion",
		.func = arttree_graph_func,
		.help = arttree_graph_help,
	},
	{
		.name = "iterate",
		.brief = "iterate over an art tree for performance",
		.func = arttree_iterate_func,
		.help = arttree_iterate_help,
	},
	{
		.name = "iterate_list",
		.brief = "iterate over the interal pmem list of leafs",
		.func = arttree_iterate_list_func,
		.help = arttree_iterate_list_help,
	},
	{
		.name = "help",
		.brief = "print help text about a command",
		.func = help_func,
		.help = help_help,
	},
	{
		.name = "search",
		.brief = "search for key in art tree",
		.func = arttree_search_func,
		.help = arttree_search_help,
	},
	{
		.name = "delete",
		.brief = "delete leaf with key from art tree",
		.func = arttree_delete_func,
		.help = arttree_delete_help,
	},
	{
		.name = "set_output",
		.brief = "set output file",
		.func = set_output_func,
		.help = set_output_help,
	},
	{
		.name = "quit",
		.brief = "quit arttree structure examiner",
		.func = quit_func,
		.help = quit_help,
	},
};


static inline unsigned long read_tsc(void)
{
	unsigned long var;
	unsigned hi, lo;

	asm volatile("rdtsc" : "=a" (lo), "=d" (hi));
	var = ((unsigned long long) hi << 32) | lo;

	return var;
}

/*
 * number of arttree_structures commands
 */
#define COMMANDS_NUMBER (sizeof(commands) / sizeof(commands[0]))

int
initialize_context(struct ds_context *ctx, int ac, char *av[])
{
	int errors = 0;
	int opt;
	char mode;

	if ((ctx == NULL) || (ac < 2)) {
		errors++;
	}

	if (!errors) {
		ctx->filename = NULL;
		ctx->newpool	= 0;
		ctx->art_tree	= NULL;
		ctx->pop	= NULL;
		ctx->fileio	= false;
		ctx->fmode	= 0666;
		ctx->mode	= 0;
		ctx->input	= stdin;
		ctx->output	= stdout;
		ctx->fd		= -1;
	}

	if (!errors) {
		while ((opt = getopt(ac, av, "s:m:n:")) != -1) {
			switch (opt) {
			case 'm':
				mode = optarg[0];
				if (mode == 'f') {
					ctx->mode |= FILL;
				} else if (mode == 'i') {
					ctx->mode |= INTERACTIVE;
				} else {
					errors++;
				}
				break;
			case 'n': {
				long operations;
				operations = strtol(optarg, NULL, 0);
				if (operations != LONG_MIN &&
				    operations != LONG_MAX) {
					ctx->operations = operations;
				}
				break;
			}
			case 's': {
				long poolsize;
				poolsize = strtol(optarg, NULL, 0);
				if (poolsize != LONG_MIN &&
				    poolsize != LONG_MAX) {
					if (poolsize > PMEMOBJ_MIN_POOL) {
						ctx->psize = poolsize;
					}
				}
				break;
			}
			default:
				errors++;
				break;
			}
		}
	}

	if (!errors) {
		ctx->filename = strdup(av[optind]);
	}


	return errors;
}

void
exit_handler(struct ds_context *ctx)
{
	(void) ctx;
}

int
art_tree_map_init(struct datastore *ds, struct ds_context *ctx)
{
	int errors = 0;
	char *error_string;

	/* calculate a required pool size */
	if (ctx->psize < PMEMOBJ_MIN_POOL)
		ctx->psize = PMEMOBJ_MIN_POOL;

	if (!ctx->fileio) {
		if (access(ctx->filename, F_OK) != 0) {
			error_string = "pmemobj_create";
			ctx->pop = pmemobj_create(ctx->filename,
				    POBJ_LAYOUT_NAME(arttree_tx),
				    ctx->psize, ctx->fmode);
			ctx->newpool = 1;
		} else {
			error_string = "pmemobj_open";
			ctx->pop = pmemobj_open(ctx->filename,
				    POBJ_LAYOUT_NAME(arttree_tx));
		}
		if (ctx->pop == NULL) {
			perror(error_string);
			errors++;
		}
	} else {
		int flags = O_CREAT | O_RDWR | O_SYNC;

		/* Create a file if it does not exist. */
		if ((ctx->fd = open(ctx->filename, flags, ctx->fmode)) < 0) {
			perror(ctx->filename);
			errors++;
		}

		/* allocate the pmem */
		if ((errno = posix_fallocate(ctx->fd, 0, ctx->psize)) != 0) {
			perror("posix_fallocate");
			errors++;
		}
	}

	if (!errors) {
		pmemobj_ds_set_priv(ds, ctx);
	} else {
		if (ctx->fileio) {
			if (ctx->fd >= 0) {
				close(ctx->fd);
			}
		} else {
			if (ctx->pop) {
				pmemobj_close(ctx->pop);
			}
		}
	}

	return errors;
}

/*
 * pmemobj_ds_set_priv -- set private structure of datastore
 */
void
pmemobj_ds_set_priv(struct datastore *ds, void *priv)
{
	ds->priv = priv;
}

struct datastore myds;

static void
usage(char *progname)
{
	printf("usage: %s -m [f|d|g]\n", progname);
	printf("  -m   mode   known modes are\n");
	printf("       f fill     create and fill art tree\n");
	printf("       i interactive     interact with art tree\n");
	printf("  -n   insertions number of key-value pairs to insert"
	    "into the tree\n");
	printf("  -s   <size>     size in bytes of the memory pool"
	    " (minimum and default: 8 MB)\n");
	printf("\nfilling an art tree is done by reading key value pairs\n"
	    "from standard input.\n"
	    "Both keys and values are single line only.\n");
}

/*
 * print_version -- prints pmempool version message
 */
static void
print_version(char *appname)
{
	printf("%s %s\n", appname, SRCVERSION);
}

/*
 * print_help -- prints pmempool help message
 */
static void
print_help(char *appname)
{
	usage(appname);
	print_version(appname);
	printf("\n");
	printf("Options:\n");
	printf("  -h, --help           display this help and exit\n");
	printf("\n");
	printf("The available commands are:\n");
	int i;
	for (i = 0; i < COMMANDS_NUMBER; i++)
		printf("%s\t- %s\n", commands[i].name, commands[i].brief);
	printf("\n");
}

/*
 * get_command -- returns command for specified command name
 */
static struct command *
get_command(char *cmd_str)
{
	int i;

	if (cmd_str == NULL) {
		return NULL;
	}

	for (i = 0; i < COMMANDS_NUMBER; i++) {
		if (strcmp(cmd_str, commands[i].name) == 0)
			return &commands[i];
	}

	return NULL;
}

/*
 * quit_help -- prints help message for quit command
 */
static void
quit_help(char *appname)
{
	printf("Usage: quit\n");
	printf("    terminate interactive arttree function\n");
}

/*
 * quit_func -- quit arttree function
 */
static int
quit_func(char *appname, struct ds_context *ctx, int argc, char *argv[])
{
	printf("\n");
	exit(0);
	return 0;
}

static void
set_output_help(char *appname)
{
	printf("set_output output redirection\n");
	printf("Usage: set_output [<file_name>]\n");
	printf("    redirect subsequent output to specified file\n");
	printf("    if file_name is not specified,"
		"then reset to standard output\n");

}

static int
set_output_func(char *appname, struct ds_context *ctx, int ac, char *av[])
{
	int errors = 0;

	if (ac == 1) {
		if ((ctx->output != NULL) && (ctx->output != stdout)) {
			(void) fclose(ctx->output);
		}
		ctx->output = stdout;
	} else if (ac == 2) {
		FILE *out_fp;

		out_fp = fopen(av[1], "w+");
		if (out_fp == (FILE *)NULL) {
			outv_err("set_output: cannot open %s for writing\n",
				av[1]);
			errors++;
		} else {
			if ((ctx->output != NULL) && (ctx->output != stdout)) {
				(void) fclose(ctx->output);
			}
			ctx->output = out_fp;
		}
	} else {
		outv_err("set_output: too many arguments [%d]\n", ac);
		errors++;
	}
	return errors;
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
help_func(char *appname, struct ds_context *ctx, int argc, char *argv[])
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

static int
arttree_fill_func(char *appname, struct ds_context *ctx, int ac, char *av[])
{
	int errors = 0;
	int opt;

	(void) appname;

	optind = 0;
	while ((opt = getopt(ac, av, "n:")) != -1) {
		switch (opt) {
		case 'n': {
			long operations;
			operations = strtol(optarg, NULL, 0);
			if (operations != LONG_MIN && operations != LONG_MAX) {
				ctx->operations = operations;
			}
			break;
			}
		default:
			errors++;
			break;
		}
	}

	if (optind >= ac) {
		outv_err("fill: missing input filename\n");
		arttree_fill_help(appname);
		errors++;
	}

	if (!errors) {
		struct stat statbuf;
		FILE *in_fp;

		if (stat(av[optind], &statbuf)) {
			outv_err("fill: cannot stat %s\n", av[optind]);
			errors++;
		} else {
			in_fp = fopen(av[optind], "r");
			if (in_fp == (FILE *)NULL) {
				outv_err("fill: cannot open %s for reading\n",
				    av[optind]);
				errors++;
			} else {
				if ((ctx->input != NULL) &&
				    (ctx->input != stdin)) {
					(void) fclose(ctx->input);
				}
				ctx->input = in_fp;
			}
		}
	}

	if (!errors) {
		if (add_elements(ctx)) {
			perror("add elements");
			errors++;
		}
		if ((ctx->input != NULL) && (ctx->input != stdin)) {
			(void) fclose(ctx->input);
		}
		ctx->input = stdin;
	}

	return errors;
}

static int
arttree_lookup_func(char *appname, struct ds_context *ctx, int ac, char *av[])
{
	int errors = 0;
	int opt;

	(void) appname;

	optind = 0;
	while ((opt = getopt(ac, av, "n:")) != -1) {
		switch (opt) {
		case 'n': {
			long operations;
			operations = strtol(optarg, NULL, 0);
			if (operations != LONG_MIN && operations != LONG_MAX) {
				ctx->operations = operations;
			}
			break;
			}
		default:
			errors++;
			break;
		}
	}

	if (optind >= ac) {
		outv_err("lookup: missing input filename\n");
		arttree_lookup_help(appname);
		errors++;
	}

	if (!errors) {
		struct stat statbuf;
		FILE *in_fp;

		if (stat(av[optind], &statbuf)) {
			outv_err("lookup: cannot stat %s\n", av[optind]);
			errors++;
		} else {
			in_fp = fopen(av[optind], "r");
			if (in_fp == (FILE *)NULL) {
				outv_err("lookup: cannot open %s for reading\n",
				    av[optind]);
				errors++;
			} else {
				if ((ctx->input != NULL) &&
				    (ctx->input != stdin)) {
					(void) fclose(ctx->input);
				}
				ctx->input = in_fp;
			}
		}
	}

	if (!errors) {
		if (lookup_elements(ctx)) {
			perror("lookup elements");
			errors++;
		}
		if ((ctx->input != NULL) && (ctx->input != stdin)) {
			(void) fclose(ctx->input);
		}
		ctx->input = stdin;
	}

	return errors;
}

static void
arttree_fill_help(char *appname)
{
	(void) appname;
	printf("create and fill an art tree\n");
	printf("Usage: fill [-n <insertions>] <input_file>\n");
	printf("   <insertions>    number of key-val pairs to fill"
		"the art tree\n");
	printf("   <input_file>    input file for key-val pairs\n");
}

static void
arttree_lookup_help(char *appname)
{
	(void) appname;
	printf("lookup keys in an art tree\n");
	printf("Usage: lookup [-n <lookup operations>] <input_file>\n");
	printf("   <lookup operations>    number of lookups to perfrom"
		"in the art tree\n");
	printf("   <input_file>    input file for keys\n");
}

static char outbuf[1024];

static char *
asciidump(unsigned char *s, int32_t len)
{
	char *p;
	int l;

	p = outbuf;
	if ((s != 0) && (len > 0)) {
		while (len--) {
			if (isprint((*s)&0xff)) {
				l = sprintf(p, "%c", (*s)&0xff);
			} else {
				l = sprintf(p, "\\%.2x", (*s)&0xff);
			}
			p += l; s++;
		}
	}
	*p = '\0'; p++;

	return outbuf;
}

static int
arttree_search_func(char *appname, struct ds_context *ctx, int ac, char *av[])
{
	void *p;
	int errors = 0;

	(void) appname;

	if (ac > 1) {
		ctx->key = (unsigned char *)strdup(av[1]);
	} else {
		outv_err("search: missing key\n");
		arttree_search_help(appname);
		errors++;
	}

	if (!errors) {
		if (ctx->output == NULL) ctx->output = stdout;
		p = art_search(ctx->pop, ctx->art_tree, ctx->key,
			    (int)strlen((const char *)ctx->key));
		if (p != NULL) {
			fprintf(ctx->output,
			    "found key [%s]: value @ 0x%lx [%s]\n",
			    asciidump(ctx->key, strlen((const char *)ctx->key)),
			    (uint64_t)p, asciidump((unsigned char *)p, 20));
		} else {
			fprintf(ctx->output,
			    "not found key [%s]\n",
			    asciidump(ctx->key,
				strlen((const char *)ctx->key)));
		}
	}

	return errors;
}

static void
arttree_search_help(char *appname)
{
	(void) appname;
	printf("search for key in art tree\n");
	printf("Usage: search <key>\n");
	printf("   <key>    the key to search for\n");
}

static int
arttree_delete_func(char *appname, struct ds_context *ctx, int ac, char *av[])
{
	void *p;
	int errors = 0;

	(void) appname;

	if (ac > 1) {
		ctx->key = (unsigned char *)strdup(av[1]);
	} else {
		outv_err("delete: missing key\n");
		arttree_delete_help(appname);
		errors++;
	}

	if (!errors) {
		if (ctx->output == NULL) ctx->output = stdout;
		p = art_delete(ctx->pop, ctx->art_tree, ctx->key,
			(int)strlen((const char *)ctx->key));
		if (p != NULL) {
			fprintf(ctx->output,
			    "delete leaf with key [%s]: value [%s]\n",
			    asciidump(ctx->key, strlen((const char *)ctx->key)),
			    asciidump((unsigned char *)p, 20));
		} else {
			fprintf(ctx->output,
			    "no leaf with key [%s]\n",
			    asciidump(ctx->key,
				strlen((const char *)ctx->key)));
		}
	}

	return errors;
}

static void
arttree_delete_help(char *appname)
{
	(void) appname;
	printf("delete leaf with key from art tree\n");
	printf("Usage: delete <key>\n");
	printf("   <key>    the key of the leaf to delete\n");
}

static int
arttree_dump_func(char *appname, struct ds_context *ctx, int ac, char *av[])
{
	(void) appname;
	(void) ac;
	(void) av;

	art_iter(ctx->pop, ctx->art_tree, dump_art_leaf_callback, NULL);

	return 0;
}

static void
arttree_dump_help(char *appname)
{
	(void) appname;
	printf("dump all leafs of an art tree\n");
	printf("Usage: dump\n");
	printf("\nThis function uses the art_iter() interface to descend\n");
	printf("to all leafs of the art tree\n");
}


static int
arttree_graph_func(char *appname, struct ds_context *ctx, int ac, char *av[])
{
	(void) appname;
	(void) ac;
	(void) av;

	fprintf(ctx->output, "digraph g {\nrankdir=LR;\n");
	art_iter2(ctx->pop, ctx->art_tree, dump_art_node_callback, NULL);
	fprintf(ctx->output, "}\n");
	return 0;
}

static void
arttree_graph_help(char *appname)
{
	(void) appname;
	printf("dump art tree for graphical output (graphiviz/dot)\n");
	printf("Usage: graph\n");
	printf("\nThis function uses the art_iter() interface to descend\n");
	printf("through the art tree and produces output for graphviz/dot\n");
}

static int
arttree_iterate_func(char *appname, struct ds_context *ctx, int ac, char *av[])
{
	unsigned long c_start;
	unsigned long c_end;
	int64_t cycles = 0;

	(void) appname;
	(void) ac;
	(void) av;

	c_start = read_tsc();
	art_iter(ctx->pop, ctx->art_tree, noop_art_tree_callback, NULL);
	c_end = read_tsc();
	cycles = (int64_t)(c_end - c_start);
	printf("performance art_iter: %ld cycles\n", cycles);
	return 0;
}

static void
arttree_iterate_help(char *appname)
{
	(void) appname;
	printf("iterate over art tree for performance\n");
	printf("Usage: iterate\n");
	printf("\nThis function uses the art_iter() interface to descend\n");
	printf("through the art tree and produces performance measurement\n");
}

static int
arttree_iterate_list_func(char *appname, struct ds_context *ctx,
	int ac, char *av[])
{
	unsigned long c_start;
	unsigned long c_end;
	int64_t cycles = 0;

	(void) appname;
	(void) ac;
	(void) av;

	c_start = read_tsc();
	art_iter_list(ctx->pop, iterate_leaf_callback, NULL);
	c_end = read_tsc();
	cycles = (int64_t)(c_end - c_start);
	printf("performance art_iter_list: %ld cycles\n", cycles);
	return 0;
}

static void
arttree_iterate_list_help(char *appname)
{
	(void) appname;
	printf("iterate over the interal pmem list of leafs\n");
	printf("Usage: iterate_list\n");
	printf("\nThis function uses the art_iter_list() "
		"interface to iterate\n");
	printf("through the pmem interal list of the leafs\n");
}

int
main(int argc, char *argv[])
{
	if (initialize_context(&my_context, argc, argv) != 0) {
		usage(argv[0]);
		return 1;
	}

	if (art_tree_map_init(&myds, &my_context) != 0) {
		fprintf(stderr, "failed to initialize memory pool file\n");
		return 1;
	}

	if (art_tree_init(&(my_context.art_tree))) {
		perror("art tree setup");
		return 1;
	}
	if ((my_context.pop != NULL) && (my_context.newpool == 0)) {
		unsigned long c_start;
		unsigned long c_end;
		int64_t cycles = 0;

		c_start = read_tsc();
		art_rebuild_tree_from_pmem_list(my_context.pop,
			my_context.art_tree);
		c_end = read_tsc();
		cycles += (int64_t)(c_end - c_start);
		printf("performance art_rebuild_tree_from_pmem_list: "
			"%ld / %ld = %ld cycles\n",
		    cycles,
		    (int64_t)(my_context.art_tree->size),
		    (int64_t)(cycles / my_context.art_tree->size));
	}

	if ((my_context.mode & INTERACTIVE)) {
		char *line;
		ssize_t read;
		size_t len;
		char *args[20];
		int nargs;
		struct command *cmdp;

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
			(void) cmdp->func(APPNAME, &my_context, nargs, args);
			printf("\n> ");
		}
		if (line != NULL) {
			free(line);
		}
	}
	if ((my_context.mode & FILL)) {
		if (add_elements(&my_context)) {
			perror("add elements");
			return 1;
		}
	}

	exit_handler(&my_context);

	return 0;
}

int
add_elements(struct ds_context *ctx)
{
	int		errors = 0;
	int		i;
	int		key_len;
	int		val_len;
	unsigned char	*key;
	unsigned char	*value;
	unsigned long c_start;
	unsigned long c_end;
	int64_t cycles = 0;

	if (ctx == NULL) {
		errors++;
	}

	if (!errors) {
		for (i = 0; i < ctx->operations; i++) {
			key = NULL;
			value = NULL;
			key_len = read_key(ctx, &key);
			val_len = read_value(ctx, &value);
			c_start = read_tsc();
			(void) art_insert(ctx->pop, ctx->art_tree,
				key, key_len, value, val_len);
			c_end = read_tsc();
			cycles += (int64_t)(c_end - c_start);
			if (key   != NULL)
				free(key);
			if (value != NULL)
				free(value);
		}
		printf("performance art_insert: %ld / %d = %ld cycles\n",
		    cycles, i, cycles / i);
	}

	return errors;
}

int
lookup_elements(struct ds_context *ctx)
{
	int		errors = 0;
	int64_t		i;
	int		key_len;
	int64_t		successful = 0;
	int64_t		failed = 0;
	unsigned char	*key;
	unsigned long c_start;
	unsigned long c_end;
	int64_t successful_cycles = 0;
	int64_t failed_cycles = 0;

	if (ctx == NULL) {
		errors++;
	}

	if (!errors) {
		for (i = 0; i < ctx->operations; i++) {
			void *result;
			key = NULL;
			key_len = read_key(ctx, &key);
			c_start = read_tsc();
			result = art_search(ctx->pop, ctx->art_tree,
					key, key_len);
			c_end = read_tsc();
			if (result == NULL) {
				failed_cycles += (int64_t)(c_end - c_start);
				failed++;
			} else {
				successful_cycles += (int64_t)(c_end - c_start);
				successful++;
			}
			if (key   != NULL)
				free(key);
		}
		printf("performance art_searchl: %ld lookups\n"
			"\tkey exists: %ld / %ld = %ld cycles\n"
			"\tkey does not exist %ld / %ld = %ld cycles\n",
			i,
			successful_cycles, successful,
			successful_cycles / successful,
			failed_cycles, failed, failed_cycles / failed);
	}

	return errors;
}

ssize_t
read_line(struct ds_context *ctx, unsigned char **line)
{
	size_t len = -1;
	ssize_t read = -1;
	*line = NULL;

	if ((read = getline((char **)line, &len, ctx->input)) > 0) {
		(*line)[read - 1] = '\0';
	}
	return read - 1;
}

static int
dump_art_leaf_callback(void *data,
	const unsigned char *key, uint32_t key_len,
	void *val, uint32_t val_len)
{
	cb_data *cbd;

	cbd = (cb_data *)data;
	/* handle only art leafs */
	if ((cbd != NULL) && (cbd->child_idx == -1)) {
		art_leaf *al;
		al = (art_leaf *)cbd->node;
		fprintf(my_context.output,
			"key len %d = [%s], value len %d [%s]\n",
			al->key_len,
			asciidump(&(al->buffer[0]), al->key_len),
			al->val_len,
			asciidump(&(al->buffer[al->key_len]), al->val_len));
		fflush(my_context.output);
	}
	return 0;
}

unsigned char hexvals[] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
	0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
	0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
	0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
	0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
	0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
	0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f,
	0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,
	0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,
	0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,
	0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
	0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
	0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f,
	0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
	0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f,
	0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,
	0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f,
	0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
	0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf,
	0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7,
	0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf,
	0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7,
	0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf,
	0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7,
	0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf,
	0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7,
	0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef,
	0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
	0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff,
};

static void
print_node_info(char *nodetype, uint64_t off, const art_node *an)
{
	int p_len, i;

	p_len = an->partial_len;
	fprintf(my_context.output,
	    "N%lx [label=\"%s at\\n0x%lx\\n%d children",
	    off, nodetype, off, an->num_children);
	if (p_len != 0) {
		fprintf(my_context.output, "\\nlen %d", p_len);
		fprintf(my_context.output, ": ");
		for (i = 0; i < p_len; i++) {
			fprintf(my_context.output, "%c", an->partial[i]);
		}
	}
	fprintf(my_context.output, "\"];\n");
}

static int
dump_art_node_callback(void *data,
	const unsigned char *key, uint32_t key_len,
	void *val, uint32_t val_len)
{
	cb_data *cbd;
	const art_node *an;
	art_node4 *an4;
	art_node16 *an16;
	art_node48 *an48;
	art_node256 *an256;
	art_node *child;

	cbd = (cb_data *)data;
	/* handle a leaf first ... */
	if ((cbd != NULL) && (cbd->child_idx == -1)) {
		unsigned char saved_char;
		art_leaf *al;
		al = (art_leaf *)cbd->node;
		fprintf(my_context.output,
		    "N%lx [shape=box, label=\"leaf at 0x%lx\n",
			(uint64_t)al, (uint64_t)al);
		saved_char = al->buffer[al->key_len];
		al->buffer[al->key_len] = '\0';
		fprintf(my_context.output, "key at 0x%lx: %s\n",
			(uint64_t)&(al->buffer[0]), &(al->buffer[0]));
		al->buffer[al->key_len] = saved_char;
		fprintf(my_context.output, "value at 0x%lx: %s\"];\n",
			(uint64_t)&(al->buffer[al->key_len]),
			&(al->buffer[al->key_len]));

	/* ... and then the tree nodes */
	} else if (cbd != NULL) {
		uint64_t child_address;
		an  = cbd->node;
		switch (cbd->node->type) {
		case NODE4:
			an4 = (art_node4 *)cbd->node;
			child = an4->children[cbd->child_idx];
			if (child != NULL) {
				print_node_info("node4",
					(uint64_t)cbd->node, an);
				if (IS_LEAF(child)) {
					PMEMoid leaf_oid;
					uint64_t leaf_off;

					leaf_off = (uint64_t)LEAF_RAW(child);
					leaf_oid.pool_uuid_lo =
					    my_context.pop->uuid_lo;
					leaf_oid.off  = leaf_off;
					child_address =
					    (uint64_t)pmemobj_direct(leaf_oid);
				} else {
					child_address = (uint64_t)child;
				}
				fprintf(my_context.output,
					"N%lx -> N%lx [label=\"%c\"];\n",
					(uint64_t)cbd->node,
					child_address,
					an4->keys[cbd->child_idx]);
			}
			break;
		case NODE16:
			an16 = (art_node16 *)cbd->node;
			child = an16->children[cbd->child_idx];
			if (child != NULL) {
				print_node_info("node16",
					(uint64_t)cbd->node, an);
				if (IS_LEAF(child)) {
					PMEMoid leaf_oid;
					uint64_t leaf_off;

					leaf_off = (uint64_t)LEAF_RAW(child);
					leaf_oid.pool_uuid_lo =
					    my_context.pop->uuid_lo;
					leaf_oid.off  = leaf_off;
					child_address =
					    (uint64_t)pmemobj_direct(leaf_oid);
				} else {
					child_address = (uint64_t)child;
				}
				fprintf(my_context.output,
					"N%lx -> N%lx [label=\"%c\"];\n",
					(uint64_t)cbd->node,
					child_address,
					an16->keys[cbd->child_idx]);
			}
			break;
		case NODE48:
			an48 = (art_node48 *)cbd->node;
			child = an48->children[cbd->child_idx];
			if (child != NULL) {
				print_node_info("node48",
					(uint64_t)cbd->node, an);
				if (IS_LEAF(child)) {
					PMEMoid leaf_oid;
					uint64_t leaf_off;

					leaf_off = (uint64_t)LEAF_RAW(child);
					leaf_oid.pool_uuid_lo =
					    my_context.pop->uuid_lo;
					leaf_oid.off  = leaf_off;
					child_address =
					    (uint64_t)pmemobj_direct(leaf_oid);
				} else {
					child_address = (uint64_t)child;
				}
				fprintf(my_context.output,
					"N%lx -> N%lx [label=\"%c\"];\n",
					(uint64_t)cbd->node,
					child_address,
					an48->keys[cbd->child_idx]);
			}
			break;
		case NODE256:
			an256 = (art_node256 *)cbd->node;
			child = an256->children[cbd->child_idx];
			if (child != NULL) {
				print_node_info("node256",
					(uint64_t)cbd->node, an);
				if (IS_LEAF(child)) {
					PMEMoid leaf_oid;
					uint64_t leaf_off;

					leaf_off = (uint64_t)LEAF_RAW(child);
					leaf_oid.pool_uuid_lo =
					    my_context.pop->uuid_lo;
					leaf_oid.off  = leaf_off;
					child_address =
					    (uint64_t)pmemobj_direct(leaf_oid);
				} else {
					child_address = (uint64_t)child;
				}
				fprintf(my_context.output,
					"N%lx -> N%lx [label=\"%c\"];\n",
					(uint64_t)cbd->node,
					child_address,
					(char)((cbd->child_idx) & 0xff));
			}
			break;
		default:
			break;
		}
	} else {
		fprintf(my_context.output,
			"leaf: key len %d = [%s], value len %d = [%s]\n",
			key_len, key, val_len, (unsigned char *)val);
	}
	return 0;
}

static int
noop_art_tree_callback(void *data,
	const unsigned char *key, uint32_t key_len,
	void *val, uint32_t val_len)
{
	(void) data;
	(void) key;
	(void) key_len;
	(void) val;

	return 0;
}

static int
iterate_leaf_callback(void *data,
	const unsigned char *key, uint32_t key_len,
	void *val, uint32_t val_len)
{
	cb_data *cbd;

	(void) key;
	(void) key_len;
	(void) val;
	(void) val_len;

	cbd = (cb_data *)data;
	/* dump leafs only */
	if ((cbd != NULL) && (cbd->child_idx == -1)) {
		unsigned char saved_char;
		art_leaf *al;
		al = (art_leaf *)cbd->node;

		fprintf(my_context.output,
		    "leaf at 0x%lx: ",
		    (uint64_t)al);
		saved_char = al->buffer[al->key_len];
		al->buffer[al->key_len] = '\0';
		fprintf(my_context.output,
		    "@ 0x%lx %s --> ",
		    (uint64_t)&(al->buffer[0]), &(al->buffer[0]));
		al->buffer[al->key_len] = saved_char;
		fprintf(my_context.output,
		    " @0x%lx %s\n",
		    (uint64_t)&(al->buffer[al->key_len]),
		    &(al->buffer[al->key_len]));
	}

	return 0;
}

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

/*
 * Copyright 2017, FUJITSU TECHNOLOGY SOLUTIONS GMBH
 * Copyright 2017, Intel Corporation
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
 *       Filename:  arttree.c
 *
 *    Description:  implement ART tree using libpmemobj based on libart
 *
 *         Author:  Andreas Bluemle, Dieter Kasper
 *                  andreas.bluemle@itxperts.de
 *                  dieter.kasper@ts.fujitsu.com
 *
 *   Organization:  FUJITSU TECHNOLOGY SOLUTIONS GMBH
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
#include "arttree.h"

#define APPNAME "arttree"
#define SRCVERSION "0.1"

/*
 * dummy structure so far; this should correspond to the datastore
 * structure as defined in examples/libpmemobj/tree_map/datastore
 */
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
	int mode;		/* operation mode */
	int operations;		/* number of insert operations to perform */
	int newpool;		/* complete new memory pool */
	size_t psize;		/* size of pool */
	PMEMobjpool *pop;	/* pmemobj handle */
	bool fileio;
	unsigned fmode;
	FILE   *input;
	FILE   *output;
	bool generate_keyvalpairs;	/* use generator for key-value pairs */
	int fd;			/* file descriptor for file io mode */
	unsigned char *key;	/* for SEARCH, INSERT and REMOVE */
	uint32_t key_len;
	unsigned char *value;	/* for INSERT */
	uint32_t val_len;
};

#define FILL 1<<1
#define INTERACTIVE 1<<3

struct _generate_parameters {
    uint64_t max_generation;
    uint64_t chunk_length;
    int	key_length;
    int	val_length;
    int	seed;
    uint64_t generation;
    uint64_t chunk_idx;
    unsigned char *key_buffer;
    unsigned char *val_buffer;
};
typedef struct _generate_parameters generate_parameters;

static generate_parameters generator =
		{1000, 100000, 40, 1024, 20161027, 0, 0, NULL, NULL};
static const unsigned char charset[] =
		"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

struct ds_context my_context;

int initialize_context(struct ds_context *ctx, int ac, char *av[]);
int add_elements(struct ds_context *ctx);
int lookup_elements(struct ds_context *ctx);
int insert_element(struct ds_context *ctx);
int search_element(struct ds_context *ctx);
int delete_element(struct ds_context *ctx);

int get_keyvalpair(struct ds_context *ctx,
		unsigned char **keyp, int *key_lenp,
		unsigned char **valp, int *val_lenp);
ssize_t read_line(struct ds_context *ctx, unsigned char **line);
void exit_handler(struct ds_context *ctx);
int art_tree_map_init(struct datastore *ds, struct ds_context *ctx);
void pmemobj_ds_set_priv(struct datastore *ds, void *priv);
static int generate(struct ds_context *ctx,
		generate_parameters *generator,
		unsigned char **keyp, int *key_lenp,
		unsigned char **valp, int *val_lenp);
static unsigned char *rand_string(unsigned char *str, uint64_t size);

static int dump_art_leaf_callback(void *data,
		const unsigned char *key, uint32_t key_len,
		const unsigned char *val, uint32_t val_len);
static int dump_art_node_callback(void *data,
		const unsigned char *key, uint32_t key_len,
		const unsigned char *val, uint32_t val_len);
static int noop_art_tree_callback(void *data,
		const unsigned char *key, uint32_t key_len,
		const unsigned char *val, uint32_t val_len);
static void print_node_info(char *nodetype, uint64_t off, const art_node *an);

#if 0
static int parse_keyval(struct ds_context *ctx, char *arg, int mode);
#endif

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
static void usage(char *progname);

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
		ctx->psize = PMEMOBJ_MIN_POOL;
		ctx->newpool = 0;
		ctx->pop = NULL;
		ctx->fileio = false;
		ctx->fmode = 0666;
		ctx->generate_keyvalpairs = false;
		ctx->mode = 0;
		ctx->input	= stdin;
		ctx->output	= stdout;
		ctx->fd = -1;
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

#if 0
static int parse_keyval(struct ds_context *ctx, char *arg, int mode)
{
	int errors = 0;
	char *p;

	p = strtok(arg, ":");
	if (p == NULL) {
		errors++;
	}

	if (!errors) {
		if (ctx->mode & (SEARCH|REMOVE|INSERT)) {
			ctx->key = (unsigned char *)strdup(p);
			assert(ctx->key != NULL);
			ctx->key_len = strlen(p);
		}
		if (ctx->mode & INSERT) {
			p = strtok(NULL, ":");
			assert(p != NULL);
			ctx->value = (unsigned char *)strdup(p);
			assert(ctx->value != NULL);
			ctx->val_len = strlen(p);
		}
	}

	return errors;
}
#endif

void
exit_handler(struct ds_context *ctx)
{
	if (!ctx->fileio) {
		if (ctx->pop) {
			pmemobj_close(ctx->pop);
		}
	} else {
		if (ctx->fd > (-1)) {
			close(ctx->fd);
		}
	}
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
print_version(char *progname)
{
	printf("%s: version %s %s with %s %s\n",
	    progname ? progname : "",
		ARTTREE_VARIANT, ARTTREE_VERSION,
	    ART_VARIANT, ART_VERSION);
}

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
		p = art_search(ctx->pop, ctx->key,
			    (int)strlen((const char *)ctx->key));
		if (p != NULL) {
			fprintf(ctx->output,
			    "found key [%s]: value @ 0x%lx [%s]\n",
			    asciidump(ctx->key, strlen((const char *)ctx->key)),
			    (uint64_t)p, asciidump((unsigned char *)p, 20));
		} else {
			fprintf(ctx->output, "not found key [%s]\n",
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
		p = art_delete(ctx->pop, ctx->key,
			(int)strlen((const char *)ctx->key));
		if (p != NULL) {
			fprintf(ctx->output, "delete leaf with key [%s]:"
					" value [%s]\n",
			    asciidump(ctx->key, strlen((const char *)ctx->key)),
			    asciidump((unsigned char *)p, 20));
		} else {
			fprintf(ctx->output, "no leaf with key [%s]\n",
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

	art_iter(ctx->pop, dump_art_leaf_callback, NULL);

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
	art_iter2(ctx->pop, dump_art_node_callback, NULL);
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
	art_iter(ctx->pop, noop_art_tree_callback, NULL);
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

	if (my_context.pop == NULL) {
		perror("pool initialization");
		return 1;
	}

	if (art_tree_init(my_context.pop, &my_context.newpool)) {
		perror("pool setup");
		return 1;
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

static int
generate(struct ds_context *ctx, generate_parameters *generator,
	unsigned char **keyp, int *key_lenp,
	unsigned char **valp, int *val_lenp)
{
	int retval = 0;

	if (generator->key_buffer == NULL) {
		srand(generator->seed);
		generator->key_buffer =
		    (unsigned char *)malloc(generator->key_length + 1);
		generator->val_buffer =
		    (unsigned char *)malloc(generator->val_length + 1);
		rand_string(generator->key_buffer, generator->key_length);
		rand_string(generator->val_buffer, generator->val_length);
	}


	if ((generator->generation >= generator->max_generation) &&
		(generator->chunk_idx >= generator->chunk_length)) {
		retval = 1;
		goto done;
	}

	if (generator->chunk_idx >= generator->chunk_length) {
		generator->generation++;
		generator->chunk_idx = 0;
		rand_string(generator->key_buffer, generator->key_length);
		rand_string(generator->val_buffer, generator->val_length);
	}
	*key_lenp = 5 + 1 + generator->key_length + 1 + 12;
	*val_lenp = (int)strlen("generation ") + 5 +
	    (int)strlen(" with string ") + generator->val_length
	    + (int)strlen(", element ") + 12;
	*keyp = (unsigned char *)malloc(*key_lenp + 1);
	*valp = (unsigned char *)malloc(*val_lenp + 1);
	generator->chunk_idx++;

	sprintf((char *)(*keyp),
	    "%.5ld-%s-%.12ld",
	    generator->generation,
	    generator->key_buffer,
	    generator->chunk_idx);
	sprintf((char *)(*valp),
	    "generation %.5ld with string %s, element %.12ld",
	    generator->generation,
	    generator->val_buffer,
	    generator->chunk_idx);

done:
	return retval;
}

static unsigned char *
rand_string(unsigned char *str, uint64_t size)
{
	uint64_t n;

	if (size) {
		--size;
		for (n = 0; n < size; n++) {
			int key = rand() % (int)(sizeof(charset) - 1);
			str[n] = charset[key];
		}
		str[size] = '\0';
	}
	return str;
}

int
get_keyvalpair(struct ds_context *ctx,
	unsigned char **keyp, int *key_lenp,
	unsigned char **valp, int *val_lenp)
{
	int retval = 0;

	if (ctx->generate_keyvalpairs == false) {
		if (*keyp != NULL) {
			free(*keyp);
			*keyp = NULL;
		}
		if (*valp != NULL) {
			free(*valp);
			*valp = NULL;
		}
		*key_lenp = read_line(ctx, keyp);
		*val_lenp = read_line(ctx, valp);
	} else {
		if (*keyp != NULL) {
			free(*keyp);
			*keyp = NULL;
		}
		if (*valp != NULL) {
			free(*valp);
			*valp = NULL;
		}
		retval = generate(ctx, &generator, keyp,
			    key_lenp, valp, val_lenp);
	}
	return retval;
}

int
add_elements(struct ds_context *ctx)
{
	PMEMobjpool *pop;
	int errors = 0;
	int i;
	int key_len;
	int val_len;
	unsigned char *key;
	unsigned char *value;
	unsigned long c_start;
	unsigned long c_end;
	int64_t cycles = 0;

	if (ctx == NULL) {
		errors++;
	} else if (ctx->pop == NULL) {
		errors++;
	}

	if (!errors) {
		pop = ctx->pop;

		for (i = 0; i < ctx->operations; i++) {
			key = NULL;
			value = NULL;
			if (get_keyvalpair(ctx, &key,
			    &key_len, &value, &val_len) != 0) break;
			c_start = read_tsc();
			art_insert(pop, key, key_len, value, val_len);
			c_end = read_tsc();
			cycles += (int64_t)(c_end - c_start);
			if (key != NULL)
				free(key);
			if (value != NULL)
				free(value);
		}
	}
	printf("performance art_insert: %ld / %d = %ld cycles\n",
	    cycles, ctx->operations,
	    ctx->operations ? cycles / ctx->operations : 0);

	return errors;
}

int
insert_element(struct ds_context *ctx)
{
	PMEMobjpool *pop;
	int errors = 0;

	if (ctx == NULL) {
		errors++;
	} else if (ctx->pop == NULL) {
		errors++;
	}

	if (!errors) {
		pop = ctx->pop;

		art_insert(pop, ctx->key, ctx->key_len,
		    ctx->value, ctx->val_len);
	}

	return errors;
}

int
lookup_elements(struct ds_context *ctx)
{
	PMEMobjpool *pop;
	int errors = 0;
	int64_t		i;
	int key_len;
	unsigned char *key;
	int64_t		successful = 0;
	int64_t		failed = 0;
	unsigned long c_start;
	unsigned long c_end;
	int64_t successful_cycles = 0;
	int64_t failed_cycles = 0;

	if (ctx == NULL) {
		errors++;
	} else if (ctx->pop == NULL) {
		errors++;
	}

	if (!errors) {
		pop = ctx->pop;

		for (i = 0; i < ctx->operations; i++) {
			void *result;
			key = NULL;
			key_len = read_line(ctx, &key);
			c_start = read_tsc();
			result = art_search(pop, key, key_len);
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
		printf("performance art_search: %ld lookups\n"
		    "\tkey exists: %ld / %ld = %ld cycles\n"
		    "\tkey does not exist %ld / %ld = %ld cycles\n",
		    i,
		    successful_cycles, successful,
		    successful_cycles / successful,
		    failed_cycles, failed,
		    failed_cycles / failed);
	}

	return errors;
}

int
search_element(struct ds_context *ctx)
{
	PMEMobjpool *pop;
	unsigned char *value;
	int errors = 0;

	if (ctx == NULL) {
		errors++;
	} else if (ctx->pop == NULL) {
		errors++;
	}

	if (!errors) {
		pop = ctx->pop;
		printf("search key [%s]: ", (char *)ctx->key);
		value = (unsigned char *)art_search(pop,
			    ctx->key, ctx->key_len);
		if (value == NULL) {
			printf("not found\n");
		} else {
			printf("value [%s]\n", value);
		}
	}

	return errors;
}

int
delete_element(struct ds_context *ctx)
{
	PMEMobjpool *pop;
	int errors = 0;

	if (ctx == NULL) {
		errors++;
	} else if (ctx->pop == NULL) {
		errors++;
	}

	if (!errors) {
		pop = ctx->pop;

		art_delete(pop, ctx->key, ctx->key_len);
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
noop_art_tree_callback(void *data,
	const unsigned char *key, uint32_t key_len,
	const unsigned char *val, uint32_t val_len)
{
	(void) data;
	(void) key;
	(void) key_len;
	(void) val;
	(void) val_len;

	return 0;
}

static int
dump_art_leaf_callback(void *data,
	const unsigned char *key, uint32_t key_len,
	const unsigned char *val, uint32_t val_len)
{
	cb_data *cbd;
	if (data != NULL) {
		cbd = (cb_data *)data;
		fprintf(my_context.output,
		    "node type %d ", (int)pmemobj_type_num(cbd->node));
		if (pmemobj_type_num(cbd->node) == art_leaf_type_num) {
			art_leaf *l = (art_leaf *)pmemobj_direct(cbd->node);
			fprintf(my_context.output,
			    "leaf key len %ld [%s], value len %ld [%s]\n",
			    l->key_len,
			    asciidump(&(l->buffer[0]), key_len),
			    l->val_len,
			    asciidump(&(l->buffer[key_len]), l->key_len));
		}
		fflush(my_context.output);
	}
	return 0;
}

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
		for (i = 0; i < p_len && i < MAX_PREFIX_LEN; i++) {
			fprintf(my_context.output, "%c", an->partial[i]);
		}
	}
	fprintf(my_context.output, "\"];\n");
}

static int
dump_art_node_callback(void *data,
	const unsigned char *key, uint32_t key_len,
	const unsigned char *val, uint32_t val_len)
{
	cb_data *cbd;
	const art_node *an;
	PMEMoid child;
	art_node4 *an4;
	art_node16 *an16;
	art_node48 *an48;
	art_node256 *an256;
	art_leaf *al;

	int type_num;

	if (data != NULL) {
		cbd = (cb_data *)data;
		type_num = pmemobj_type_num(cbd->node);
		if (type_num == art_node4_type_num) {
			an4 = (art_node4 *)pmemobj_direct(cbd->node);
			an = &(an4->n);
			child = an4->children[cbd->child_idx];
			if (!OID_IS_NULL(child)) {
				print_node_info("node4",
				    cbd->node.off, an);
				fprintf(my_context.output,
				    "N%lx -> N%lx [label=\"%c\"];\n",
				    cbd->node.off,
				    child.off,
				    an4->keys[cbd->child_idx]);
			}
		} else if (type_num == art_node16_type_num) {
			an16 = (art_node16 *)pmemobj_direct(cbd->node);
			an = &(an16->n);
			child = an16->children[cbd->child_idx];
			if (!OID_IS_NULL(child)) {
				print_node_info("node16",
				    cbd->node.off, an);
				fprintf(my_context.output,
				    "N%lx -> N%lx [label=\"%c\"];\n",
				    cbd->node.off,
				    child.off,
				    an16->keys[cbd->child_idx]);
			}
		} else if (type_num == art_node48_type_num) {
			an48 = (art_node48 *)pmemobj_direct(cbd->node);
			an = &(an48->n);
			child = an48->children[cbd->child_idx];
			if (!OID_IS_NULL(child)) {
				print_node_info("node48",
				    cbd->node.off, an);
				fprintf(my_context.output,
				    "N%lx -> N%lx [label=\"%c\"];\n",
				    cbd->node.off,
				    child.off,
				    an48->keys[cbd->child_idx]);
			}
		} else if (type_num == art_node256_type_num) {
			an256 = (art_node256 *)pmemobj_direct(cbd->node);
			an = &(an256->n);
			child = an256->children[cbd->child_idx];
			if (!OID_IS_NULL(child)) {
				print_node_info("node256",
				    cbd->node.off, an);
				fprintf(my_context.output,
				    "N%lx -> N%lx [label=\"0x%x\"];\n",
				    cbd->node.off,
				    child.off,
				    ((cbd->child_idx) & 0xff));
			}
		} else if (type_num == art_leaf_type_num) {
			al = (art_leaf *)pmemobj_direct(cbd->node);
			// oid_key = &(al->buffer[0]);
			// oid_value = &(al->buffer[al->key_len]);
			fprintf(my_context.output, "N%lx [shape=box,"
				"label=\"leaf at\\n0x%lx\"];\n",
			    cbd->node.off, cbd->node.off);
			fprintf(my_context.output, "N%lx [shape=box,"
				"label=\"key at 0x%lx: %s\"];\n",
			    cbd->node.off + offsetof(art_leaf, buffer) + 0,
			    cbd->node.off + offsetof(art_leaf, buffer) + 0,
			    asciidump(al->buffer, al->key_len));
			fprintf(my_context.output, "N%lx [shape=box,"
				"label=\"value at 0x%lx: %s\"];\n",
			    cbd->node.off +
				    offsetof(art_leaf, buffer) +
				    al->key_len,
			    cbd->node.off +
				    offsetof(art_leaf, buffer) +
				    al->key_len,
			    asciidump(&(al->buffer[key_len]),
				    al->val_len));
			fprintf(my_context.output, "N%lx -> N%lx;\n",
			    cbd->node.off,
			    cbd->node.off + offsetof(art_leaf, buffer));
			fprintf(my_context.output, "N%lx -> N%lx;\n",
			    cbd->node.off,
			    cbd->node.off +
				    offsetof(art_leaf, buffer) +
				    al->key_len);
		}
	} else {
		fprintf(my_context.output,
		    "leaf: key len %d = [%s], value len %d = [%s]\n",
		    key_len, key, val_len, val);
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

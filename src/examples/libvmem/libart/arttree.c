/*
 * Copyright 2016, FUJITSU TECHNOLOGY SOLUTIONS GMBH
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
 * ===========================================================================
 *
 *       Filename:  arttree.c
 *
 *    Description:  implement ART tree using libpmemobj based on libart
 *
 *       Author:  Andreas Bluemle, Dieter Kasper
 *                Andreas.Bluemle.external@ts.fujitsu.com
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
#include "libvmem.h"
#include "arttree.h"

#define APPNAME "arttree"
#define SRCVERSION "0.1"

struct str2int_map {
	char *name;
	int value;
};

#define ART_NODE	0
#define ART_NODE4	1
#define ART_NODE16	2
#define ART_NODE48	3
#define ART_NODE256	4
#define ART_TREE_ROOT	5
#define ART_LEAF	6

struct str2int_map art_node_types[] = {
	{"art_node", ART_NODE},
	{"art_node4", ART_NODE4},
	{"art_node16", ART_NODE16},
	{"art_node48", ART_NODE48},
	{"art_node256", ART_NODE256},
	{"art_tree", ART_TREE_ROOT},
	{"art_leaf", ART_LEAF},
	{NULL, -1}
};

struct datastore
{
	void *priv;
};

/*
 * context - main context of datastore
 */
struct ds_context
{
	char *dirname;		/* name of pool file */
	int mode;		/* operation mode */
	int insertions;		/* number of insert operations to perform */
	int newpool;		/* complete new memory pool */
	size_t psize;		/* size of pool */
	VMEM *vmp;		/* handle to vmem pool */
	art_tree *art_tree;	/* art_tree root */
	bool fileio;
	unsigned int fmode;
	FILE *input;
	FILE *output;
	uint64_t address;
	unsigned char *key;
	int type;
	int fd;			/* file descriptor for file io mode */
};

#define FILL (1 << 1)
#define INTERACTIVE (1 << 3)

struct ds_context my_context;

#define read_key(c, p) read_line(c, p)
#define read_value(c, p) read_line(c, p)

static void usage(char *progname);
int initialize_context(struct ds_context *ctx, int ac, char *av[]);
int add_elements(struct ds_context *ctx);
ssize_t read_line(struct ds_context *ctx, unsigned char **line);
void exit_handler(struct ds_context *ctx);
int art_tree_map_init(struct datastore *ds, struct ds_context *ctx);
void pmemobj_ds_set_priv(struct datastore *ds, void *priv);
static int dump_art_leaf_callback(void *data, const unsigned char *key,
		uint32_t key_len, const unsigned char *val, uint32_t val_len);
static int dump_art_tree_graph(void *data, const unsigned char *key,
		uint32_t key_len, const unsigned char *val, uint32_t val_len);
static void print_node_info(char *nodetype, uint64_t addr, art_node *an);

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
static int arttree_examine_func(char *appname, struct ds_context *ctx,
		int ac, char *av[]);
static void arttree_examine_help(char *appname);
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
static int map_lookup(struct str2int_map *map, char *name);

static void arttree_examine(struct ds_context *ctx, void *addr, int node_type);
static void dump_art_tree_root(struct ds_context *ctx, art_tree *node);
static void dump_art_node(struct ds_context *ctx, art_node *node);
static void dump_art_node4(struct ds_context *ctx, art_node4 *node);
static void dump_art_node16(struct ds_context *ctx, art_node16 *node);
static void dump_art_node48(struct ds_context *ctx, art_node48 *node);
static void dump_art_node256(struct ds_context *ctx, art_node256 *node);
static void dump_art_leaf(struct ds_context *ctx, art_leaf *node);
static char *asciidump(unsigned char *s, int32_t len);

void outv_err(const char *fmt, ...);
void outv_err_vargs(const char *fmt, va_list ap);

/*
 * command -- struct for commands definition
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
		.name = "help",
		.brief = "print help text about a command",
		.func = help_func,
		.help = help_help,
	},
	{
		.name = "examine",
		.brief = "examine art tree structures",
		.func = arttree_examine_func,
		.help = arttree_examine_help,
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
		ctx->dirname	= NULL;
		ctx->psize	= VMEM_MIN_POOL;
		ctx->newpool	= 0;
		ctx->vmp	= NULL;
		ctx->art_tree	= NULL;
		ctx->fileio	= false;
		ctx->fmode	= 0666;
		ctx->mode	= 0;
		ctx->input	= stdin;
		ctx->output	= stdout;
		ctx->fd		= -1;
	}

	if (!errors) {
		while ((opt = getopt(ac, av, "m:n:s:")) != -1) {
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
				long int insertions;
				insertions = strtol(optarg, NULL, 0);
				if (insertions > 0 && insertions < LONG_MAX) {
					ctx->insertions = insertions;
				}
				break;

			}
			default:
				errors++;
				break;
			}
		}
	}

	if (optind >= ac) {
		errors++;
	}
	if (!errors) {
		ctx->dirname = strdup(av[optind]);
	}

	return errors;
}

void
exit_handler(struct ds_context *ctx)
{
	if (!ctx->fileio) {
		if (ctx->vmp) {
			vmem_delete(ctx->vmp);
		}
	} else {
		if (ctx->fd > - 1) {
			close(ctx->fd);
		}
	}
}

int
art_tree_map_init(struct datastore *ds, struct ds_context *ctx)
{
	int errors = 0;

	/* calculate a required pool size */
	if (ctx->psize < VMEM_MIN_POOL)
		ctx->psize = VMEM_MIN_POOL;

	if (!ctx->fileio) {
		if (access(ctx->dirname, F_OK) == 0) {
			ctx->vmp = vmem_create(ctx->dirname, ctx->psize);
			if (ctx->vmp == NULL) {
				perror("vmem_create");
				errors++;
			}
			ctx->newpool = 1;
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
	printf("usage: %s -m [f|d|g] dir\n", progname);
	printf("  -m   mode   known modes are\n");
	printf("       f fill     create and fill art tree\n");
	printf("       i interactive     interact with art tree\n");
	printf("  -n   insertions number of key-value pairs to insert"
	    "into the tree\n");
	printf("  -s   size       size of the vmem pool file "
	    "[minimum: VMEM_MIN_POOL=%ld]\n", VMEM_MIN_POOL);
	printf("\nfilling an art tree is done by reading key value pairs\n"
	    "from standard input.\n"
	    "Both keys and values are single line only.\n");
}

/*
 * print_version -- prints arttree version message
 */
static void
print_version(char *appname)
{
	printf("%s %s\n", appname, SRCVERSION);
}

/*
 * print_help -- prints arttree help message
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

static int
map_lookup(struct str2int_map *map, char *name)
{
	int idx;
	int value = -1;

	for (idx = 0; ; idx++) {
		if (map[idx].name == NULL) {
			break;
		}
		if (strcmp((const char *)map[idx].name,
			    (const char *)name) == 0) {
			value = map[idx].value;
			break;
		}
	}
	return value;
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
			long int insertions;
			insertions = strtol(optarg, NULL, 0);
			if (insertions > 0 && insertions < LONG_MAX) {
				ctx->insertions = insertions;
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
			p += l;
			s++;
		}
	}
	*p = '\0';
	p++;

	return outbuf;
}

static void
dump_art_tree_root(struct ds_context *ctx, art_tree *node)
{
	fprintf(ctx->output, "art_tree 0x%lx {\n"
		"   size=%ld;\n   root=0x%lx;\n}\n",
	    (uint64_t)node, node->size, (uint64_t)(node->root));
}

static void
dump_art_node(struct ds_context *ctx, art_node *node)
{
	fprintf(ctx->output,	"art_node 0x%lx {\n"
				"   type=%s;\n"
				"   num_children=%d;\n"
				"   partial_len=%d;\n"
				"   partial=[%s];\n"
				"}\n",
	    (uint64_t)node, art_node_types[node->type].name,
	    node->num_children, node->partial_len,
	    asciidump(node->partial, node->partial_len));
}

static void
dump_art_node4(struct ds_context *ctx, art_node4 *node)
{
	int i;

	fprintf(ctx->output, "art_node4 0x%lx {\n", (uint64_t)node);
	dump_art_node(ctx, &(node->n));
	for (i = 0; i < node->n.num_children; i++) {
		fprintf(ctx->output, "   key[%d]=%s;\n",
		    i, asciidump(&(node->keys[i]), 1));
		fprintf(ctx->output, "   child[%d]=0x%lx;\n",
		    i, (uint64_t)(node->children[i]));
	}
	fprintf(ctx->output, "}\n");
}

static void
dump_art_node16(struct ds_context *ctx, art_node16 *node)
{
	int i;

	fprintf(ctx->output, "art_node16 0x%lx {\n", (uint64_t)node);
	dump_art_node(ctx, &(node->n));
	for (i = 0; i < node->n.num_children; i++) {
		fprintf(ctx->output, "   key[%d]=%s;\n",
		    i, asciidump(&(node->keys[i]), 1));
		fprintf(ctx->output, "   child[%d]=0x%lx;\n",
		    i, (uint64_t)(node->children[i]));
	}
	fprintf(ctx->output, "}\n");
}

static void
dump_art_node48(struct ds_context *ctx, art_node48 *node)
{
	int i;
	int idx;

	fprintf(ctx->output, "art_node48 0x%lx {\n", (uint64_t)node);
	dump_art_node(ctx, &(node->n));
	for (i = 0; i < 256; i++) {
		idx = node->keys[i];
		if (!idx)
			continue;

		fprintf(ctx->output, "   key[%d]=%s;\n",
		    i, asciidump((unsigned char *)(&i), 1));
		fprintf(ctx->output, "   child[%d]=0x%lx;\n",
		    idx, (uint64_t)(node->children[idx]));
	}
	fprintf(ctx->output, "}\n");
}

static void
dump_art_node256(struct ds_context *ctx, art_node256 *node)
{
	int i;

	fprintf(ctx->output, "art_node48 0x%lx {\n", (uint64_t)node);
	dump_art_node(ctx, &(node->n));
	for (i = 0; i < 256; i++) {
		if (node->children[i] == NULL)
			continue;

		fprintf(ctx->output, "   key[%i]=%s;\n",
		    i, asciidump((unsigned char *)(&i), 1));
		fprintf(ctx->output, "   child[%d]=0x%lx;\n",
		    i, (uint64_t)(node->children[i]));
	}
	fprintf(ctx->output, "}\n");
}

static void
dump_art_leaf(struct ds_context *ctx, art_leaf *node)
{
	fprintf(ctx->output,	"art_leaf 0x%lx {\n"
				"   key_len=%u;\n"
				"   key=[%s];\n"
				"   val_len=%u;\n"
				"   value=[%s];\n"
				"}\n",
	    (uint64_t)node,
	    node->key_len, asciidump(node->key, (int32_t)node->key_len),
	    node->val_len, asciidump(node->value, (int32_t)node->val_len));
}

static void
arttree_examine(struct ds_context *ctx, void *addr, int node_type)
{
	if (addr == NULL)
		return;

	switch (node_type) {
	case ART_TREE_ROOT:
		dump_art_tree_root(ctx, (art_tree *)addr);
		break;
	case ART_NODE:
		dump_art_node(ctx, (art_node *)addr);
		break;
	case ART_NODE4:
		dump_art_node4(ctx, (art_node4 *)addr);
		break;
	case ART_NODE16:
		dump_art_node16(ctx, (art_node16 *)addr);
		break;
	case ART_NODE48:
		dump_art_node48(ctx, (art_node48 *)addr);
		break;
	case ART_NODE256:
		dump_art_node256(ctx, (art_node256 *)addr);
		break;
	case ART_LEAF:
		dump_art_leaf(ctx, (art_leaf *)addr);
		break;
	default: break;
	}
	fflush(ctx->output);
}

static int
arttree_examine_func(char *appname, struct ds_context *ctx, int ac, char *av[])
{
	int errors = 0;

	(void) appname;

	if (ac > 1) {
		if (ac < 3) {
			outv_err("examine: missing argument\n");
			arttree_examine_help(appname);
			errors++;
		} else {
			ctx->address = (uint64_t)strtol(av[1], NULL, 0);
			ctx->type = map_lookup(&(art_node_types[0]), av[2]);
		}
	} else {
		ctx->address = (uint64_t)ctx->art_tree;
		ctx->type = ART_TREE_ROOT;
	}

	if (!errors) {
		if (ctx->output == NULL)
			ctx->output = stdout;
		arttree_examine(ctx, (void *)(ctx->address), ctx->type);
	}

	return errors;
}

static void
arttree_examine_help(char *appname)
{
	(void) appname;

	printf("examine structures of an art tree\n");
	printf("Usage: examine <address> <type>\n");
	printf("   <address>    address of art tree structure to examine\n");
	printf("   <type>       input file for key-val pairs\n");
	printf("Known types are\n   art_tree\n   art_node\n"
	    "   art_node4\n   art_node16\n   art_node48\n   art_node256\n"
	    "   art_leaf\n");
	printf("If invoked without arguments, then the root of the art tree"
	    " is dumped\n");
}

static int
arttree_search_func(char *appname, struct ds_context *ctx, int ac, char *av[])
{
	void *p;
	int errors = 0;

	(void) appname;

	if (ac > 1) {
		ctx->key = (unsigned char *)strdup(av[1]);
		assert(ctx->key != NULL);
	} else {
		outv_err("search: missing key\n");
		arttree_search_help(appname);
		errors++;
	}

	if (!errors) {
		if (ctx->output == NULL)
			ctx->output = stdout;
		p = art_search(ctx->art_tree, ctx->key,
			    (int)strlen((const char *)ctx->key));
		if (p != NULL) {
			fprintf(ctx->output, "found key [%s]: value [%s]\n",
			    asciidump(ctx->key, strlen((const char *)ctx->key)),
			    asciidump((unsigned char *)p, 20));
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
		assert(ctx->key != NULL);
	} else {
		outv_err("delete: missing key\n");
		arttree_delete_help(appname);
		errors++;
	}

	if (!errors) {
		if (ctx->output == NULL) ctx->output = stdout;
		p = art_delete(ctx->vmp, ctx->art_tree, ctx->key,
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

	art_iter(ctx->art_tree, dump_art_leaf_callback, NULL);
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
	art_iter2(ctx->art_tree, dump_art_tree_graph, NULL);
	fprintf(ctx->output, "}\n");
	return 0;
}

static void
arttree_graph_help(char *appname)
{
	(void) appname;

	printf("dump art tree for graphical output (graphiviz/dot)\n");
	printf("Usage: graph\n");
	printf("\nThis function uses the art_iter2() interface to descend\n");
	printf("through the art tree and produces output for graphviz/dot\n");
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

	if (my_context.vmp == NULL) {
		perror("pool initialization");
		return 1;
	}

	my_context.art_tree = (art_tree *)vmem_malloc(my_context.vmp,
						sizeof(art_tree));
	assert(my_context.art_tree != NULL);
	if (art_tree_init(my_context.art_tree)) {
		perror("art tree setup");
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

int
add_elements(struct ds_context *ctx)
{
	int errors = 0;
	int i;
	int key_len;
	int val_len;
	unsigned char *key;
	unsigned char *value;

	if (ctx == NULL) {
		errors++;
	} else if (ctx->vmp == NULL) {
		errors++;
	}

	if (!errors) {
		for (i = 0; i < ctx->insertions; i++) {
			key = NULL;
			value = NULL;
			key_len = read_key(ctx, &key);
			val_len = read_value(ctx, &value);
			art_insert(ctx->vmp, ctx->art_tree,
				key, key_len, value, val_len);
			if (key   != NULL)
				free(key);
			if (value != NULL)
				free(value);
		}
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
	const unsigned char *val, uint32_t val_len)
{
	fprintf(my_context.output, "key len %d = [%s], value len %d = [%s]\n",
		key_len, asciidump((unsigned char *)key, key_len),
		val_len, asciidump((unsigned char *)val, val_len));
	fflush(my_context.output);
	return 0;
}

/*
 * Macros to manipulate pointer tags
 */
#define IS_LEAF(x) (((uintptr_t)x & 1))
#define SET_LEAF(x) ((void *)((uintptr_t)x | 1))
#define LEAF_RAW(x) ((void *)((uintptr_t)x & ~1))

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
print_node_info(char *nodetype, uint64_t addr, art_node *an)
{
	int p_len;

	p_len = an->partial_len;
	fprintf(my_context.output,
	    "N%lx [label=\"%s at\\n0x%lx\\n%d children",
	    addr, nodetype, addr, an->num_children);
	if (p_len != 0) {
		fprintf(my_context.output, "\\nlen %d", p_len);
		fprintf(my_context.output, ": ");
		asciidump(an->partial, p_len);
	}
	fprintf(my_context.output, "\"];\n");
}

static int
dump_art_tree_graph(void *data,
	const unsigned char *key, uint32_t key_len,
	const unsigned char *val, uint32_t val_len)
{
	cb_data *cbd;
	art_node4 *an4;
	art_node16 *an16;
	art_node48 *an48;
	art_node256 *an256;
	art_leaf *al;
	void *child;
	int idx;

	if (data == NULL)
		return 0;

	cbd = (cb_data *)data;

	if (IS_LEAF(cbd->node)) {
		al = LEAF_RAW(cbd->node);
		fprintf(my_context.output,
			"N%lx [shape=box, label=\"leaf at\\n0x%lx\"];\n",
			(uint64_t)al, (uint64_t)al);
		fprintf(my_context.output,
			"N%lx [shape=box, label=\"key at 0x%lx: %s\"];\n",
			(uint64_t)al->key, (uint64_t)al->key,
			asciidump(al->key, al->key_len));
		fprintf(my_context.output,
			"N%lx [shape=box, label=\"value at 0x%lx: %s\"];\n",
			(uint64_t)al->value, (uint64_t)al->value,
			asciidump(al->value, al->val_len));
		fprintf(my_context.output,
			"N%lx -> N%lx;\n",
			(uint64_t)al, (uint64_t)al->key);
		fprintf(my_context.output,
			"N%lx -> N%lx;\n",
			(uint64_t)al, (uint64_t)al->value);
		return 0;
	}

	switch (cbd->node_type) {
	case NODE4:
		an4 = (art_node4 *)cbd->node;
		child = (void *)(an4->children[cbd->child_idx]);
		child = LEAF_RAW(child);
		if (child != NULL) {
			if (cbd->first_child)
				print_node_info("node4",
				    (uint64_t)(cbd->node), &(an4->n));
			fprintf(my_context.output,
			    "N%lx -> N%lx [label=\"%s\"];\n",
			    (uint64_t)an4, (uint64_t)child,
			    asciidump(&(an4->keys[cbd->child_idx]), 1));
		}
		break;
	case NODE16:
		an16 = (art_node16 *)cbd->node;
		child = (void *)(an16->children[cbd->child_idx]);
		child = LEAF_RAW(child);
		if (child != NULL) {
			if (cbd->first_child)
				print_node_info("node16",
				    (uint64_t)(cbd->node), &(an16->n));
			fprintf(my_context.output,
			    "N%lx -> N%lx [label=\"%s\"];\n",
			    (uint64_t)an16, (uint64_t)child,
			    asciidump(&(an16->keys[cbd->child_idx]), 1));
		}
		break;
	case NODE48:
		an48 = (art_node48 *)cbd->node;
		idx = an48->keys[cbd->child_idx];
		child = (void *) (an48->children[idx - 1]);
		child = LEAF_RAW(child);
		if (child != NULL) {
			if (cbd->first_child)
				print_node_info("node48",
				    (uint64_t)(cbd->node), &(an48->n));
			fprintf(my_context.output,
			    "N%lx -> N%lx [label=\"%s\"];\n",
			    (uint64_t)an48, (uint64_t)child,
			    asciidump(&(hexvals[cbd->child_idx]), 1));
		}
		break;
	case NODE256:
		an256 = (art_node256 *)cbd->node;
		child = (void *)(an256->children[cbd->child_idx]);
		child = LEAF_RAW(child);
		if (child != NULL) {
			if (cbd->first_child)
				print_node_info("node256",
				    (uint64_t)(cbd->node), &(an256->n));
			fprintf(my_context.output,
			    "N%lx -> N%lx [label=\"%s\"];\n",
			    (uint64_t)an256, (uint64_t)child,
			    asciidump(&(hexvals[cbd->child_idx]), 1));
		}
		break;
	default:
		break;
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

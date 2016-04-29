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
 *       Filename:  arttree_examine.c
 *
 *    Description:  implementation of examine function for ART tree structures
 *
 *         Author:  Andreas Bluemle, Dieter Kasper
 *                  Andreas.Bluemle.external@ts.fujitsu.com
 *                  dieter.kasper@ts.fujitsu.com
 *
 *   Organization:  FUJITSU TECHNOLOGY SOLUTIONS GMBH
 *
 * ===========================================================================
 */

#include <stdio.h>
#include <libgen.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/mman.h>
#include "arttree_structures.h"

/*
 * examine context
 */
struct examine_ctx {
    struct pmem_context *pmem_ctx;
    char *offset_string;
    uint64_t offset;
    char *type_name;
    int32_t  type;
    int32_t  hexdump;
};

static struct examine_ctx *ex_ctx = NULL;

struct examine {
	const char *name;
	const char *brief;
	int (*func)(char *, struct examine_ctx *, off_t);
	void (*help)(char *);
};

/*  local functions */
static int examine_parse_args(char *appname, int ac, char *av[],
		struct examine_ctx *ex_ctx);
static struct examine *get_examine(char *type_name);
static void print_usage(char *appname);

static void dump_PMEMoid(char *prefix, PMEMoid *oid);

static int examine_PMEMoid(char *appname, struct examine_ctx *ctx, off_t off);
static int examine_art_tree_root(char *appname,
		struct examine_ctx *ctx, off_t off);
static int examine_art_node_u(char *appname,
		struct examine_ctx *ctx, off_t off);
static int examine_art_node4(char *appname,
		struct examine_ctx *ctx, off_t off);
static int examine_art_node16(char *appname,
		struct examine_ctx *ctx, off_t off);
static int examine_art_node48(char *appname,
		struct examine_ctx *ctx, off_t off);
static int examine_art_node256(char *appname,
		struct examine_ctx *ctx, off_t off);
#if 0 /* XXX */
static int examine_art_node(char *appname,
		struct examine_ctx *ctx, off_t off);
#else
static int examine_art_node(art_node *an);
#endif
static int examine_art_leaf(char *appname,
		struct examine_ctx *ctx, off_t off);
static int examine_var_string(char *appname,
		struct examine_ctx *ctx, off_t off);

/* global visible interface */
void arttree_examine_help(char *appname);
int arttree_examine_func(char *appname,
		struct pmem_context *ctx, int ac, char *av[]);

const char *arttree_examine_help_str =
"Examine data structures (objects) of ART tree\n"
"Arguments: <offset> <type>\n"
"   <offset> offset of object in pmem file\n"
"   <type>   one of art_tree_root, art_node_u, art_node,"
" art_node4, art_node16, art_node48, art_node256, art_leaf\n"
;

static const struct option long_options[] = {
	{"hexdump",	no_argument,	0,	'x'},
	{0,		0,		0,	 0 },
};

struct examine ex_funcs[] = {
	{
		.name = "PMEMobj",
		.brief = "examine PMEMoid structure",
		.func = examine_PMEMoid,
		.help = NULL,
	},
	{
		.name = "art_tree_root",
		.brief = "examine art_tree_root structure",
		.func = examine_art_tree_root,
		.help = NULL,
	},
	{
		.name = "art_node_u",
		.brief = "examine art_node_u structure",
		.func = examine_art_node_u,
		.help = NULL,
	},
	{
		.name = "art_node4",
		.brief = "examine art_node4 structure",
		.func = examine_art_node4,
		.help = NULL,
	},
	{
		.name = "art_node16",
		.brief = "examine art_node16 structure",
		.func = examine_art_node16,
		.help = NULL,
	},
	{
		.name = "art_node48",
		.brief = "examine art_node48 structure",
		.func = examine_art_node48,
		.help = NULL,
	},
	{
		.name = "art_node256",
		.brief = "examine art_node256 structure",
		.func = examine_art_node256,
		.help = NULL,
	},
	{
		.name = "art_leaf",
		.brief = "examine art_leaf structure",
		.func = examine_art_leaf,
		.help = NULL,
	},
	{
		.name = "var_string",
		.brief = "examine var_string structure",
		.func = examine_var_string,
		.help = NULL,
	},
};

/*
 * number of arttree examine commands
 */
#define COMMANDS_NUMBER (sizeof(ex_funcs) / sizeof(ex_funcs[0]))

void
arttree_examine_help(char *appname)
{
	printf("%s %s\n", appname, arttree_examine_help_str);
}

int
arttree_examine_func(char *appname, struct pmem_context *ctx,
	int ac, char *av[])
{
	int errors = 0;
	off_t offset;
	struct examine *ex;

	if (ctx == NULL) {
		return -1;
	}

	if (ex_ctx == NULL) {
		ex_ctx = (struct examine_ctx *)
		    calloc(1, sizeof(struct examine_ctx));
		if (ex_ctx == NULL) {
			return -1;
		}
	}

	ex_ctx->pmem_ctx = ctx;
	if (examine_parse_args(appname, ac, av, ex_ctx) != 0) {
		fprintf(stderr, "%s::%s: error parsing arguments\n",
		    appname, __FUNCTION__);
		errors++;
	}

	if (!errors) {
		offset = (off_t)strtol(ex_ctx->offset_string, NULL, 0);
		ex = get_examine(ex_ctx->type_name);
		if (ex != NULL) {
			ex->func(appname, ex_ctx, offset);
		}
	}
	return errors;
}

static int
examine_parse_args(char *appname, int ac, char *av[],
	struct examine_ctx *ex_ctx)
{
	int ret = 0;
	int opt;

	optind = 0;
	while ((opt = getopt_long(ac, av, "x", long_options, NULL)) != -1) {
		switch (opt) {
		case 'x':
			ex_ctx->hexdump = 1;
			break;
		default:
			print_usage(appname);
			ret = 1;
		}
	}
	if (ret == 0) {
		ex_ctx->offset_string = strdup(av[optind + 0]);
		ex_ctx->type_name = strdup(av[optind + 1]);
	}

	return ret;
}

static void
print_usage(char *appname)
{
	printf("%s: examine <offset> <type>\n", appname);
}

/*
 * get_command -- returns command for specified command name
 */
static struct examine *
get_examine(char *type_name)
{
	int i;

	if (type_name == NULL) {
		return NULL;
	}

	for (i = 0; i < COMMANDS_NUMBER; i++) {
		if (strcmp(type_name, ex_funcs[i].name) == 0)
			return &ex_funcs[i];
	}

	return NULL;
}

static void
dump_PMEMoid(char *prefix, PMEMoid *oid)
{
	printf("%s { PMEMoid pool_uuid_lo %lx off 0x%lx = %ld }\n",
	    prefix, oid->pool_uuid_lo, oid->off, oid->off);
}

static int
examine_PMEMoid(char *appname, struct examine_ctx *ctx, off_t off)
{
	int fd;
	int errors = 0;
	void *p;
	size_t obj_len;

	fd = ctx->pmem_ctx->fd;
	obj_len = sizeof(PMEMoid);
	p = mmap(NULL, off + obj_len, PROT_READ, MAP_SHARED, fd, 0);
	if (p != MAP_FAILED) {
		dump_PMEMoid("PMEMoid", p);
	} else {
		perror("examine_PMEMoid mmap failed");
		errors++;
	}

	return errors;
}

static int
examine_art_tree_root(char *appname, struct examine_ctx *ctx, off_t off)
{
	int fd;
	int errors = 0;
	void *p;
	size_t obj_len;
	art_tree_root *tree_root;

	fd = ctx->pmem_ctx->fd;
	obj_len = sizeof(art_tree_root);
	p = mmap(NULL, off + obj_len, PROT_READ, MAP_SHARED, fd, 0);
	if (p != MAP_FAILED) {
		tree_root = (art_tree_root *)(((unsigned char *)p) + off);
		printf("at offset 0x%lx, art_tree_root {\n", off);
		printf("    size %d\n", tree_root->size);
		dump_PMEMoid("    art_node_u", (PMEMoid *)&(tree_root->root));
		printf("\n};\n");
	} else {
		perror("examine_art_tree_root mmap failed");
		errors++;
	}

	return errors;
}

static int
examine_art_node_u(char *appname, struct examine_ctx *ctx, off_t off)
{
	int fd;
	int errors = 0;
	void *p;
	size_t obj_len;
	art_node_u *node_u;

	fd = ctx->pmem_ctx->fd;
	obj_len = sizeof(art_node_u);
	p = mmap(NULL, off + obj_len, PROT_READ, MAP_SHARED, fd, 0);
	if (p != MAP_FAILED) {
		node_u = (art_node_u *)(((unsigned char *)p) + off);
		printf("at offset 0x%lx, art_node_u {\n", off);
		printf("    type %d [%s]\n", node_u->art_node_type,
		    art_node_names[node_u->art_node_type]);
		printf("    tag %d\n", node_u->art_node_tag);
		switch (node_u->art_node_type) {
		case ART_NODE4:
			dump_PMEMoid("    art_node4 oid",
			    &(node_u->u.an4.oid));
			break;
		case ART_NODE16:
			dump_PMEMoid("    art_node16 oid",
			    &(node_u->u.an16.oid));
			break;
		case ART_NODE48:
			dump_PMEMoid("    art_node48 oid",
			    &(node_u->u.an48.oid));
			break;
		case ART_NODE256:
			dump_PMEMoid("    art_node256 oid",
			    &(node_u->u.an256.oid));
			break;
		case ART_LEAF:
			dump_PMEMoid("    art_leaf oid",
			    &(node_u->u.al.oid));
			break;
		default: printf("ERROR: unkown node type\n");
			break;
		}
		printf("\n};\n");
	} else {
		perror("examine_art_node_u mmap failed");
		errors++;
	}

	return errors;
}

static int
examine_art_node4(char *appname, struct examine_ctx *ctx, off_t off)
{
	int fd;
	int errors = 0;
	int i;
	void *p;
	size_t obj_len;
	art_node4 *an4;

	fd = ctx->pmem_ctx->fd;
	obj_len = sizeof(art_node4);
	p = mmap(NULL, off + obj_len, PROT_READ, MAP_SHARED, fd, 0);
	if (p != MAP_FAILED) {
		an4 = (art_node4 *)(((unsigned char *)p) + off);
		printf("at offset 0x%lx, art_node4 {\n", off);
		examine_art_node(&(an4->n));
		printf("keys [");
		for (i = 0; i < 4; i++) {
			printf("%c ", an4->keys[i]);
		}
		printf("]\nnodes [\n");
		for (i = 0; i < 4; i++) {
			dump_PMEMoid("       art_node_u oid",
			    &(an4->children[i].oid));
		}
		printf("\n]");
		printf("\n};\n");
	} else {
		perror("examine_art_node4 mmap failed");
		errors++;
	}

	return errors;
}

static int
examine_art_node16(char *appname, struct examine_ctx *ctx, off_t off)
{
	int fd;
	int errors = 0;
	int i;
	void *p;
	size_t obj_len;
	art_node16 *an16;

	fd = ctx->pmem_ctx->fd;
	obj_len = sizeof(art_node16);
	p = mmap(NULL, off + obj_len, PROT_READ, MAP_SHARED, fd, 0);
	if (p != MAP_FAILED) {
		an16 = (art_node16 *)(((unsigned char *)p) + off);
		printf("at offset 0x%lx, art_node16 {\n", off);
		examine_art_node(&(an16->n));
		printf("keys [");
		for (i = 0; i < 16; i++) {
			printf("%c ", an16->keys[i]);
		}
		printf("]\nnodes [\n");
		for (i = 0; i < 16; i++) {
			dump_PMEMoid("       art_node_u oid",
			    &(an16->children[i].oid));
		}
		printf("\n]");
		printf("\n};\n");
	} else {
		perror("examine_art_node16 mmap failed");
		errors++;
	}

	return errors;
}

static int
examine_art_node48(char *appname, struct examine_ctx *ctx, off_t off)
{
	int fd;
	int errors = 0;
	int i;
	void *p;
	size_t obj_len;
	art_node48 *an48;

	fd = ctx->pmem_ctx->fd;
	obj_len = sizeof(art_node48);
	p = mmap(NULL, off + obj_len, PROT_READ, MAP_SHARED, fd, 0);
	if (p != MAP_FAILED) {
		an48 = (art_node48 *)(((unsigned char *)p) + off);
		printf("at offset 0x%lx, art_node48 {\n", off);
		examine_art_node(&(an48->n));
		printf("keys [");
		for (i = 0; i < 256; i++) {
			printf("%c ", an48->keys[i]);
		}
		printf("]\nnodes [\n");
		for (i = 0; i < 48; i++) {
			dump_PMEMoid("       art_node_u oid",
			    &(an48->children[i].oid));
		}
		printf("\n]");
		printf("\n};\n");
	} else {
		perror("examine_art_node48 mmap failed");
		errors++;
	}

	return errors;
}

static int
examine_art_node256(char *appname, struct examine_ctx *ctx, off_t off)
{
	int fd;
	int errors = 0;
	int i;
	void *p;
	size_t obj_len;
	art_node256 *an256;

	fd = ctx->pmem_ctx->fd;
	obj_len = sizeof(art_node256);
	p = mmap(NULL, off + obj_len, PROT_READ, MAP_SHARED, fd, 0);
	if (p != MAP_FAILED) {
		an256 = (art_node256 *)(((unsigned char *)p) + off);
		printf("at offset 0x%lx, art_node256 {\n", off);
		examine_art_node(&(an256->n));
		printf("nodes [\n");
		for (i = 0; i < 256; i++) {
			dump_PMEMoid("       art_node_u oid",
			    &(an256->children[i].oid));
		}
		printf("\n]");
		printf("\n};\n");
	} else {
		perror("examine_art_node256 mmap failed");
		errors++;
	}

	return errors;
}

#if 0 /* XXX */
static int
examine_art_node(char *appname, struct examine_ctx *ctx, off_t off)
{
	int fd;
	int errors = 0;
	int i;
	void *p;
	size_t obj_len;
	art_node *an;

	fd = ctx->pmem_ctx->fd;
	obj_len = sizeof(art_node);
	p = mmap(NULL, off + obj_len, PROT_READ, MAP_SHARED, fd, 0);
	if (p != MAP_FAILED) {
		an = (art_node *)(((unsigned char *)p) + off);
		printf("at offset 0x%lx, art_node {\n", off);
		printf("     num_children  %d\n", an->num_children);
		printf("     partial_len   %d\n", an->partial_len);
		printf("     partial [");
		for (i = 0; i < 10; i++) {
			printf("%c ", an->partial[i]);
		}
		printf("\n]");
		printf("\n};\n");
	} else {
		perror("examine_art_node mmap failed");
		errors++;
	}

	return errors;
}
#else
static int
examine_art_node(art_node *an)
{
	int i;

	printf("art_node {\n");
	printf("     num_children  %d\n", an->num_children);
	printf("     partial_len   %d\n", an->partial_len);
	printf("     partial [");
	for (i = 0; i < 10; i++) {
		printf("%c ", an->partial[i]);
	}
	printf("\n]");
	printf("\n};\n");

	return 0;
}
#endif

static int
examine_art_leaf(char *appname, struct examine_ctx *ctx, off_t off)
{
	int fd;
	int errors = 0;
	void *p;
	size_t obj_len;
	art_leaf *al;

	fd = ctx->pmem_ctx->fd;
	obj_len = sizeof(art_leaf);
	p = mmap(NULL, off + obj_len, PROT_READ, MAP_SHARED, fd, 0);
	if (p != MAP_FAILED) {
		al = (art_leaf *)(((unsigned char *)p) + off);
		printf("at offset 0x%lx, art_leaf {\n", off);
		dump_PMEMoid("       var_string key oid  ", &(al->key.oid));
		dump_PMEMoid("       var_string value oid", &(al->value.oid));
		printf("\n};\n");
	} else {
		perror("examine_art_leaf mmap failed");
		errors++;
	}

	return errors;
}

static int
examine_var_string(char *appname, struct examine_ctx *ctx, off_t off)
{
	int fd;
	int errors = 0;
	void *p;
	size_t obj_len;
	var_string *vs;

	fd = ctx->pmem_ctx->fd;
	obj_len = sizeof(var_string);
	p = mmap(NULL, off + obj_len, PROT_READ, MAP_SHARED, fd, 0);
	if (p != MAP_FAILED) {
		vs = (var_string *)(((unsigned char *)p) + off);
		printf("at offset 0x%lx, var_string {\n", off);
		printf("    len %ld s [%s]", vs->len, vs->s);
		printf("\n};\n");
	} else {
		perror("examine_var_string mmap failed");
		errors++;
	}

	return errors;
}

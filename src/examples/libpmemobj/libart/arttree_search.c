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
 *       Filename:  arttree_search.c
 *
 *    Description:  implementation of search function for ART tree
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
#include <inttypes.h>
#include <libgen.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <sys/mman.h>
#include "arttree_structures.h"

/*
 * search context
 */
struct search_ctx {
    struct pmem_context *pmem_ctx;
    unsigned char *search_key;
    int32_t  hexdump;
};

struct search {
	const char *name;
	const char *brief;
	char *(*func)(char *, struct search_ctx *);
	void (*help)(char *);
};

/*  local functions */
static int search_parse_args(char *appname, int ac, char *av[],
		struct search_ctx *s_ctx);
static struct search *get_search(char *type_name);
static void print_usage(char *appname);

static void dump_PMEMoid(char *prefix, PMEMoid *oid);

static char *search_key(char *appname, struct search_ctx *ctx);
static int leaf_matches(struct search_ctx *ctx, art_leaf *n,
		unsigned char *key, size_t key_len, int depth);
static int check_prefix(art_node *an,
		unsigned char *key, int key_len, int depth);
static uint64_t find_child(art_node *n, int node_type, unsigned char key);

static void *get_node(struct search_ctx *ctx, int node_type, uint64_t off);
static uint64_t get_offset_an(art_node_u *au);
static void dump_PMEMoid(char *prefix, PMEMoid *oid);
static void dump_art_tree_root(char *prefix, uint64_t off, void *p);

/* global visible interface */
void arttree_search_help(char *appname);
int arttree_search_func(char *appname, struct pmem_context *ctx,
		int ac, char *av[]);

static const char *arttree_search_help_str =
"Search for key in ART tree\n"
"Arguments: <key>\n"
"   <key> key\n"
;

static const struct option long_options[] = {
	{"hexdump",	no_argument,	NULL,	'x'},
	{NULL,		0,		NULL,	 0 },
};

static struct search s_funcs[] = {
	{
		.name = "key",
		.brief = "search for key",
		.func = search_key,
		.help = NULL,
	}
};

/* Simple inlined function */
static inline int
min(int a, int b)
{
	return (a < b) ? b : a;
}

/*
 * number of arttree examine commands
 */
#define COMMANDS_NUMBER (sizeof(s_funcs) / sizeof(s_funcs[0]))

void
arttree_search_help(char *appname)
{
	printf("%s %s\n", appname, arttree_search_help_str);
}

int
arttree_search_func(char *appname, struct pmem_context *ctx, int ac, char *av[])
{
	int errors = 0;
	struct search *s;
	char *value;
	struct search_ctx s_ctx;

	value = NULL;
	if (ctx == NULL) {
		return -1;
	}

	memset(&s_ctx, 0, sizeof(struct search_ctx));

	if (ctx->art_tree_root_offset == 0) {
		fprintf(stderr, "search functions require knowledge"
			    " about the art_tree_root.\n");
		fprintf(stderr, "Use \"set_root <offset>\""
			    " to define where the \nart_tree_root object"
			    " resides in the pmem file.\n");
		errors++;
	}

	s_ctx.pmem_ctx = ctx;

	if (search_parse_args(appname, ac, av, &s_ctx) != 0) {
		fprintf(stderr, "%s::%s: error parsing arguments\n",
		    appname, __FUNCTION__);
		errors++;
	}

	if (!errors) {
		s = get_search("key");
		if (s != NULL) {
			value = s->func(appname, &s_ctx);
		}
		if (value != NULL) {
			printf("key [%s] found, value [%s]\n",
			    s_ctx.search_key, value);
		} else {
			printf("key [%s] not found\n", s_ctx.search_key);
		}
	}

	if (s_ctx.search_key != NULL)
		free(s_ctx.search_key);

	return errors;
}

static int
search_parse_args(char *appname, int ac, char *av[], struct search_ctx *s_ctx)
{
	int ret = 0;
	int opt;

	optind = 0;
	while ((opt = getopt_long(ac, av, "x", long_options, NULL)) != -1) {
		switch (opt) {
		case 'x':
			s_ctx->hexdump = 1;
			break;
		default:
			print_usage(appname);
			ret = 1;
		}
	}
	if (ret == 0) {
		s_ctx->search_key = (unsigned char *)strdup(av[optind + 0]);
	}

	return ret;
}

static void
print_usage(char *appname)
{
	printf("%s: search <key>\n", appname);
}

/*
 * get_search -- returns command for specified command name
 */
static struct search *
get_search(char *type_name)
{
	if (type_name == NULL) {
		return NULL;
	}

	for (size_t i = 0; i < COMMANDS_NUMBER; i++) {
		if (strcmp(type_name, s_funcs[i].name) == 0)
			return &s_funcs[i];
	}

	return NULL;
}

static void *
get_node(struct search_ctx *ctx, int node_type, uint64_t off)
{
	if (!VALID_NODE_TYPE(node_type))
		return NULL;

	printf("%s at off 0x%" PRIx64 "\n", art_node_names[node_type], off);

	return ctx->pmem_ctx->addr + off;
}

static int
leaf_matches(struct search_ctx *ctx, art_leaf *n,
	unsigned char *key, size_t key_len, int depth)
{
	var_string *n_key;
	(void) depth;

	n_key = (var_string *)get_node(ctx, VAR_STRING, n->key.oid.off);
	if (n_key == NULL)
		return 1;

	// HACK for stupid null-terminated strings....
	// else if (n_key->len != key_len)
	//	ret = 1;
	if (n_key->len != key_len + 1)
		return 1;

	return memcmp(n_key->s, key, key_len);
}

static int
check_prefix(art_node *n, unsigned char *key, int key_len, int depth)
{
	int max_cmp = min(min(n->partial_len, MAX_PREFIX_LEN), key_len - depth);
	int idx;

	for (idx = 0; idx < max_cmp; idx++) {
		if (n->partial[idx] != key[depth + idx])
			return idx;
	}
	return idx;
}

static uint64_t
find_child(art_node *n, int node_type, unsigned char c)
{
	int i;
	union {
		art_node4 *p1;
		art_node16 *p2;
		art_node48 *p3;
		art_node256 *p4;
	} p;

	printf("[%s] children %d search key %c [",
	    art_node_names[node_type], n->num_children, c);
	switch (node_type) {
	case ART_NODE4:
		p.p1 = (art_node4 *)n;
		for (i = 0; i < n->num_children; i++) {
			printf("%c ", p.p1->keys[i]);
			if (p.p1->keys[i] == c) {
				printf("]\n");
				return p.p1->children[i].oid.off;
			}
		}
		break;
	case ART_NODE16:
		p.p2 = (art_node16 *)n;
		for (i = 0; i < n->num_children; i++) {
			printf("%c ", p.p2->keys[i]);
			if (p.p2->keys[i] == c) {
				printf("]\n");
				return p.p2->children[i].oid.off;
			}
		}
		break;
	case ART_NODE48:
		p.p3 = (art_node48 *)n;
		i = p.p3->keys[c];
		printf("%d ", p.p3->keys[c]);
		if (i) {
			printf("]\n");
			return p.p3->children[i - 1].oid.off;
		}
		break;

	case ART_NODE256:
		p.p4 = (art_node256 *)n;
		printf("0x%" PRIx64, p.p4->children[c].oid.off);
		if (p.p4->children[c].oid.off != 0) {
			printf("]\n");
			return p.p4->children[c].oid.off;
		}
		break;
	default:
		abort();
	}
	printf("]\n");
	return 0;
}

static uint64_t
get_offset_an(art_node_u *au)
{
	uint64_t offset = 0;

	switch (au->art_node_type) {
	case ART_NODE4:
		offset = au->u.an4.oid.off;
		break;
	case ART_NODE16:
		offset = au->u.an16.oid.off;
		break;
	case ART_NODE48:
		offset = au->u.an48.oid.off;
		break;
	case ART_NODE256:
		offset = au->u.an256.oid.off;
		break;
	case ART_LEAF:
		offset = au->u.al.oid.off;
		break;
	default:
		break;
	}

	return offset;
}

static char *
search_key(char *appname, struct search_ctx *ctx)
{
	int errors = 0;
	void *p;		/* something */
	off_t p_off;
	art_node_u *p_au;	/* art_node_u */
	off_t p_au_off;
	void *p_an;		/* specific art node from art_node_u */
	off_t p_an_off;
	art_node *an;		/* art node */
	var_string *n_value;
	char *value;
	int prefix_len;
	int depth = 0;
	int key_len;
	uint64_t child_off;

	key_len = strlen((char *)(ctx->search_key));
	value = NULL;

	p_off = ctx->pmem_ctx->art_tree_root_offset;
	p = get_node(ctx, ART_TREE_ROOT, p_off);
	assert(p != NULL);

	dump_art_tree_root("art_tree_root", p_off, p);
	p_au_off = ((art_tree_root *)p)->root.oid.off;
	p_au = (art_node_u *)get_node(ctx, ART_NODE_U, p_au_off);
	if (p_au == NULL)
		errors++;

	if (!errors) {
		while (p_au) {
			p_an_off = get_offset_an(p_au);
			p_an = get_node(ctx, p_au->art_node_type, p_an_off);
			assert(p_an != NULL);
			if (p_au->art_node_type == ART_LEAF) {
				if (!leaf_matches(ctx, (art_leaf *)p_an,
				    ctx->search_key, key_len, depth)) {
					n_value = (var_string *)
					    get_node(ctx, VAR_STRING,
					    ((art_leaf *)p_an)->value.oid.off);
					return (char *)(n_value->s);
				}
			}
			an = (art_node *)p_an;
			if (an->partial_len) {
			    prefix_len = check_prefix(an, ctx->search_key,
					    key_len, depth);
				if (prefix_len !=
				    min(MAX_PREFIX_LEN, an->partial_len)) {
					return NULL;
				}
				depth = depth + an->partial_len;
			}
			child_off = find_child(an, p_au->art_node_type,
				    ctx->search_key[depth]);

			if (child_off != 0) {
				p_au_off = child_off;
				p_au = get_node(ctx, ART_NODE_U, p_au_off);
			} else {
				p_au = NULL;
			}
			depth++;
		}
	}

	if (errors) {
		return NULL;
	} else {
		return value;
	}
}

static void
dump_art_tree_root(char *prefix, uint64_t off, void *p)
{
	art_tree_root *tree_root;
	tree_root = (art_tree_root *)p;
	printf("at offset 0x%" PRIx64 ", art_tree_root {\n", off);
	printf("    size %d\n", tree_root->size);
	dump_PMEMoid("    art_node_u", (PMEMoid *)&(tree_root->root));
	printf("\n};\n");
}

static void
dump_PMEMoid(char *prefix, PMEMoid *oid)
{
	printf("%s { PMEMoid pool_uuid_lo %" PRIx64
	    " off 0x%" PRIx64 " = %" PRId64 " }\n",
	    prefix, oid->pool_uuid_lo, oid->off, oid->off);
}

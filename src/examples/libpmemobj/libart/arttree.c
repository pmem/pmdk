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
 *       Filename:  arttree.c
 *
 *    Description:  implement ART tree using libpmemobj based on libart
 *
 *         Author:  Andreas Bluemle, Dieter Kasper
 *                  Andreas.Bluemle.external@ts.fujitsu.com
 *                  dieter.kasper@ts.fujitsu.com
 *
 *   Organization:  FUJITSU TECHNOLOGY SOLUTIONS GMBH
 *
 * ===========================================================================
 */

#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#ifdef __FreeBSD__
#define _WITH_GETLINE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <stdbool.h>
#include <inttypes.h>
#include <fcntl.h>
#include <emmintrin.h>
#include <sys/types.h>
#include <sys/mman.h>
#include "libpmemobj.h"
#include "arttree.h"

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
	int insertions;		/* number of insert operations to perform */
	int newpool;		/* complete new memory pool */
	size_t psize;		/* size of pool */
	PMEMobjpool *pop;	/* pmemobj handle */
	bool fileio;
	unsigned fmode;
	int fd;			/* file descriptor for file io mode */
	char *addr;		/* base mapping address for file io mode */
	unsigned char *key;	/* for SEARCH, INSERT and REMOVE */
	uint32_t key_len;
	unsigned char *value;	/* for INSERT */
	uint32_t val_len;
};

#define FILL (1 << 1)
#define DUMP (1 << 2)
#define GRAPH (1 << 3)
#define INSERT (1 << 4)
#define SEARCH (1 << 5)
#define REMOVE (1 << 6)

struct ds_context my_context;

extern TOID(var_string) null_var_string;
extern TOID(art_leaf)   null_art_leaf;
extern TOID(art_node_u) null_art_node_u;

#define read_key(p) read_line(p)
#define read_value(p) read_line(p)

int initialize_context(struct ds_context *ctx, int ac, char *av[]);
int initialize_pool(struct ds_context *ctx);
int add_elements(struct ds_context *ctx);
int insert_element(struct ds_context *ctx);
int search_element(struct ds_context *ctx);
int delete_element(struct ds_context *ctx);
ssize_t read_line(unsigned char **line);
void exit_handler(struct ds_context *ctx);
int art_tree_map_init(struct datastore *ds, struct ds_context *ctx);
void pmemobj_ds_set_priv(struct datastore *ds, void *priv);
static int dump_art_leaf_callback(void *data,
		const unsigned char *key, uint32_t key_len,
		const unsigned char *val, uint32_t val_len);
static int dump_art_node_callback(void *data,
		const unsigned char *key, uint32_t key_len,
		const unsigned char *val, uint32_t val_len);
static void print_node_info(char *nodetype, uint64_t off, const art_node *an);
static int parse_keyval(struct ds_context *ctx, char *arg, int mode);

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
		ctx->mode = 0;
		ctx->fd = -1;
	}

	if (!errors) {
		while ((opt = getopt(ac, av, "s:m:n:")) != -1) {
			switch (opt) {
			case 'm':
				mode = optarg[0];
				if (mode == 'f') {
					ctx->mode |= FILL;
				} else if (mode == 'd') {
					ctx->mode |= DUMP;
				} else if (mode == 'g') {
					ctx->mode |= GRAPH;
				} else if (mode == 'i') {
					ctx->mode |= INSERT;
					parse_keyval(ctx, av[optind], INSERT);
					optind++;
				} else if (mode == 's') {
					ctx->mode |= SEARCH;
					parse_keyval(ctx, av[optind], SEARCH);
					optind++;
				} else if (mode == 'r') {
					ctx->mode |= REMOVE;
					parse_keyval(ctx, av[optind], REMOVE);
					optind++;
				} else {
					errors++;
				}
				break;
			case 'n': {
				long insertions;
				insertions = strtol(optarg, NULL, 0);
				if (insertions > 0 && insertions < LONG_MAX) {
					ctx->insertions = insertions;
				}
				break;
			}
			case 's': {
				long poolsize;
				poolsize = strtol(optarg, NULL, 0);
				if (poolsize >= PMEMOBJ_MIN_POOL) {
					ctx->psize = poolsize;
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
			ctx->key_len = strlen(p) + 1;
		}
		if (ctx->mode & INSERT) {
			p = strtok(NULL, ":");
			assert(p != NULL);
			ctx->value = (unsigned char *)strdup(p);
			assert(ctx->value != NULL);
			ctx->val_len = strlen(p) + 1;
		}
	}

	return errors;
}

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

		/* map file to memory */
		if ((ctx->addr = mmap(NULL, ctx->psize, PROT_READ, MAP_SHARED,
				ctx->fd, 0)) == MAP_FAILED) {
			perror("mmap");
			errors++;
		}
	}

	if (!errors) {
		pmemobj_ds_set_priv(ds, ctx);
	} else {
		if (ctx->fileio) {
			if (ctx->addr != NULL) {
				munmap(ctx->addr, ctx->psize);
			}
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
	printf("usage: %s -m [f|d|g] file\n", progname);
	printf("  -m   mode   known modes are\n");
	printf("       f fill     create and fill art tree\n");
	printf("       i insert   insert an element into the art tree\n");
	printf("       s search   search for a key in the art tree\n");
	printf("       r remove   remove an element from the art tree\n");
	printf("       d dump     dump art tree\n");
	printf("       g graph    dump art tree as a graphviz dot graph\n");
	printf("  -n   <number>   number of key-value pairs to insert"
	    " into the art tree\n");
	printf("  -s   <size>     size in bytes of the memory pool"
	    " (minimum and default: 8 MB)");
	printf("\nfilling an art tree is done by reading key-value pairs\n"
	    "from standard input.\n"
	    "Both keys and values are single line only.\n");
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

	if ((my_context.mode & FILL)) {
		if (add_elements(&my_context)) {
			perror("add elements");
			return 1;
		}
	}

	if ((my_context.mode & INSERT)) {
		if (insert_element(&my_context)) {
			perror("insert elements");
			return 1;
		}
	}

	if ((my_context.mode & SEARCH)) {
		if (search_element(&my_context)) {
			perror("search elements");
			return 1;
		}
	}

	if ((my_context.mode & REMOVE)) {
		if (delete_element(&my_context)) {
			perror("delete elements");
			return 1;
		}
	}

	if (my_context.mode & DUMP) {
		art_iter(my_context.pop, dump_art_leaf_callback, NULL);
	}

	if (my_context.mode & GRAPH) {
		printf("digraph g {\nrankdir=LR;\n");
		art_iter(my_context.pop, dump_art_node_callback, NULL);
		printf("}");
	}

	exit_handler(&my_context);

	return 0;
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

	if (ctx == NULL) {
		errors++;
	} else if (ctx->pop == NULL) {
		errors++;
	}

	if (!errors) {
		pop = ctx->pop;

		for (i = 0; i < ctx->insertions; i++) {
			key = NULL;
			value = NULL;
			key_len = read_key(&key);
			val_len = read_value(&value);
			art_insert(pop, key, key_len, value, val_len);
			if (key != NULL)
				free(key);
			if (value != NULL)
				free(value);
		}
	}

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
search_element(struct ds_context *ctx)
{
	PMEMobjpool *pop;
	TOID(var_string) value;
	int errors = 0;

	if (ctx == NULL) {
		errors++;
	} else if (ctx->pop == NULL) {
		errors++;
	}

	if (!errors) {
		pop = ctx->pop;
		printf("search key [%s]: ", (char *)ctx->key);
		value = art_search(pop, ctx->key, ctx->key_len);
		if (TOID_IS_NULL(value)) {
			printf("not found\n");
		} else {
			printf("value [%s]\n", D_RO(value)->s);
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
read_line(unsigned char **line)
{
	size_t len = -1;
	ssize_t read = -1;
	*line = NULL;

	if ((read = getline((char **)line, &len, stdin)) > 0) {
		(*line)[read - 1] = '\0';
	}
	return read;
}

static int
dump_art_leaf_callback(void *data,
	const unsigned char *key, uint32_t key_len,
	const unsigned char *val, uint32_t val_len)
{
	cb_data *cbd;
	if (data != NULL) {
		cbd = (cb_data *)data;
		printf("node type %d ", D_RO(cbd->node)->art_node_type);
		if (D_RO(cbd->node)->art_node_type == art_leaf_t) {
			printf("key len %" PRIu32 " = [%s], value len %" PRIu32
			    " = [%s]",
			    key_len,
			    key != NULL ? (char *)key : (char *)"NULL",
			    val_len,
			    val != NULL ? (char *)val : (char *)"NULL");
		}
		printf("\n");
	} else {
		printf("key len %" PRIu32 " = [%s], value len %" PRIu32
		    " = [%s]\n",
		    key_len,
		    key != NULL ? (char *)key : (char *)"NULL",
		    val_len,
		    val != NULL ? (char *)val : (char *)"NULL");
	}
	return 0;
}

static void
print_node_info(char *nodetype, uint64_t off, const art_node *an)
{
	int p_len, i;

	p_len = an->partial_len;
	printf("N%" PRIx64 " [label=\"%s at\\n0x%" PRIx64 "\\n%d children",
	    off, nodetype, off, an->num_children);
	if (p_len != 0) {
		printf("\\nlen %d", p_len);
		printf(": ");
		for (i = 0; i < p_len; i++) {
			printf("%c", an->partial[i]);
		}
	}
	printf("\"];\n");
}

static int
dump_art_node_callback(void *data,
	const unsigned char *key, uint32_t key_len,
	const unsigned char *val, uint32_t val_len)
{
	cb_data *cbd;
	const art_node *an;
	TOID(art_node4) an4;
	TOID(art_node16) an16;
	TOID(art_node48) an48;
	TOID(art_node256) an256;
	TOID(art_leaf) al;
	TOID(art_node_u) child;
	TOID(var_string) oid_key;
	TOID(var_string) oid_value;

	if (data != NULL) {
		cbd = (cb_data *)data;
		switch (D_RO(cbd->node)->art_node_type) {
		case NODE4:
			an4 = D_RO(cbd->node)->u.an4;
			an = &(D_RO(an4)->n);
			child = D_RO(an4)->children[cbd->child_idx];
			if (!TOID_IS_NULL(child)) {
				print_node_info("node4",
				    cbd->node.oid.off, an);
				printf("N%" PRIx64 " -> N%" PRIx64
				    " [label=\"%c\"];\n",
				    cbd->node.oid.off,
				    child.oid.off,
				    D_RO(an4)->keys[cbd->child_idx]);
			}
			break;
		case NODE16:
			an16 = D_RO(cbd->node)->u.an16;
			an = &(D_RO(an16)->n);
			child = D_RO(an16)->children[cbd->child_idx];
			if (!TOID_IS_NULL(child)) {
				print_node_info("node16",
				    cbd->node.oid.off, an);
				printf("N%" PRIx64 " -> N%" PRIx64
				    " [label=\"%c\"];\n",
				    cbd->node.oid.off,
				    child.oid.off,
				    D_RO(an16)->keys[cbd->child_idx]);
			}
			break;
		case NODE48:
			an48 = D_RO(cbd->node)->u.an48;
			an = &(D_RO(an48)->n);
			child = D_RO(an48)->children[cbd->child_idx];
			if (!TOID_IS_NULL(child)) {
				print_node_info("node48",
				    cbd->node.oid.off, an);
				printf("N%" PRIx64 " -> N%" PRIx64
				    " [label=\"%c\"];\n",
				    cbd->node.oid.off,
				    child.oid.off,
				    D_RO(an48)->keys[cbd->child_idx]);
			}
			break;
		case NODE256:
			an256 = D_RO(cbd->node)->u.an256;
			an = &(D_RO(an256)->n);
			child = D_RO(an256)->children[cbd->child_idx];
			if (!TOID_IS_NULL(child)) {
				print_node_info("node256",
				    cbd->node.oid.off, an);
				printf("N%" PRIx64 " -> N%" PRIx64
				    " [label=\"0x%x\"];\n",
				    cbd->node.oid.off,
				    child.oid.off,
				    (char)((cbd->child_idx) & 0xff));
			}
			break;
		case art_leaf_t:
			al = D_RO(cbd->node)->u.al;
			oid_key = D_RO(al)->key;
			oid_value = D_RO(al)->value;
			printf("N%" PRIx64 " [shape=box,"
				"label=\"leaf at\\n0x%" PRIx64 "\"];\n",
			    cbd->node.oid.off, cbd->node.oid.off);
			printf("N%" PRIx64 " [shape=box,"
				"label=\"key at 0x%" PRIx64 ": %s\"];\n",
			    oid_key.oid.off, oid_key.oid.off,
			    D_RO(oid_key)->s);
			printf("N%" PRIx64 " [shape=box,"
				"label=\"value at 0x%" PRIx64 ": %s\"];\n",
			    oid_value.oid.off, oid_value.oid.off,
			    D_RO(oid_value)->s);
			printf("N%" PRIx64 " -> N%" PRIx64 ";\n",
			    cbd->node.oid.off, oid_key.oid.off);
			printf("N%" PRIx64 " -> N%" PRIx64 ";\n",
			    cbd->node.oid.off, oid_value.oid.off);
			break;
		default:
			break;
		}
	} else {
		printf("leaf: key len %" PRIu32
		    " = [%s], value len %" PRIu32 " = [%s]\n",
		    key_len, key, val_len, val);
	}
	return 0;
}

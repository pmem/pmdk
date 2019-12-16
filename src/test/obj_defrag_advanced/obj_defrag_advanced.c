/*
 * Copyright 2020, Intel Corporation
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
 * obj_defrag_advanced.c -- test for libpmemobj defragmentation feature
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stddef.h>
#include <unistd.h>
#include <stdlib.h>

#include <getopt.h>

#include "vgraph.h"
#include "pgraph.h"
#include "unittest.h"

/* operation to perform */
enum op {
	OP_CREATE, /* create a graph */
	OP_DUMP, /* dump graph to a file */
	OP_DEFRAG, /* defragment the pool containing a graph */
	OP_MAX
};

struct task {
	enum op op;
	const char *path;
	const char *dump;
	unsigned seed;

	struct vgraph_params vparams;
	struct pgraph_params pparams;
};

/*
 * graph_defrag -- defragment the pool
 * - collect pointers to all PMEMoids
 * - do a sanity checks
 * - call pmemobj_defrag
 */
static void
graph_defrag(PMEMobjpool *pop, struct pgraph_t *pgraph)
{
	/* count number of oids */
	unsigned oidcnt = pgraph->nodes_num;
	for (unsigned i = 0; i < pgraph->nodes_num; ++i) {
		struct pnode *pnode = (struct pnode *)pmemobj_direct
				(pgraph->nodes[i]);
		oidcnt += pnode->edges_num;
	}

	/* create array of oid pointers */
	PMEMoid **oidv = (PMEMoid **)malloc(sizeof(PMEMoid *) * oidcnt);
	unsigned oidi = 0;
	for (unsigned i = 0; i < pgraph->nodes_num; ++i) {
		oidv[oidi++] = &pgraph->nodes[i];

		struct pnode *pnode = (struct pnode *)pmemobj_direct
				(pgraph->nodes[i]);
		for (unsigned j = 0; j < pnode->edges_num; ++j) {
			oidv[oidi++] = &pnode->edges[j];
		}
	}

	UT_ASSERTeq(oidi, oidcnt);

	/* check if all oids are valid */
	for (unsigned i = 0; i < oidcnt; ++i) {
		void *ptr = pmemobj_direct(*oidv[i]);
		UT_ASSERTne(ptr, NULL);
	}

	/* check if all oids appear only once */
	for (unsigned i = 0; i < oidcnt - 1; ++i) {
		for (unsigned j = i + 1; j < oidcnt; ++j) {
			UT_ASSERTne(oidv[i], oidv[j]);
		}
	}

	struct pobj_defrag_result result;
	int ret = pmemobj_defrag(pop, oidv, oidcnt, &result);
	UT_ASSERTeq(ret, 0);

	free(oidv);
}

#define TEST_NAME "obj_defrag_advanced"

/*
 * print_usage -- print usage of the program
 */
static void
print_usage()
{
	printf(TEST_NAME " --create --path pool [opts]\n"
			"to create a graph in a <pool> where the following [opts]"
			" are available:\n"
			"\t--max-nodes n_max\t- maximum number of nodes\n"
			"\t--max-edges e_max\t- maximum number of edges per node\n"
			"\t--graph_copies c_num\t- number of graph copies to choose the"
			" nodes from\n"
			"\t--seed s\t\t- pseudo-random generator seed\n"
			"\n"
			TEST_NAME " --dump file --path pool\n"
			"to dump a graph stored in a <pool> to a text <file>.\n"
			"\n"
			TEST_NAME " --defrag --path pool\n"
			"to defragment the <pool> having a graph.\n"
			"\n"
			TEST_NAME " --help\n"
			"to display this help message.\n"
			"\n");

}

#define GRAPH_LAYOUT POBJ_LAYOUT_NAME(graph)

/*
 * create_op -- create a graph
 * - generate an intermediate volatile graph representation
 * - use the volatile graph to allocate a persistent one
 */
static void
create_op(struct task *task)
{
	if (task->path == NULL) {
		print_usage();
		exit(EXIT_FAILURE);
	}

	PMEMobjpool *pop = NULL;

	pop = pmemobj_create(task->path, GRAPH_LAYOUT, 0, S_IWUSR | S_IRUSR);
	if (pop == NULL) {
		UT_FATAL("!pmemobj_create: %s", task->path);
	}

	srand(task->seed);

	struct vgraph_t *vgraph = vgraph_new(&task->vparams);
	(void) pgraph_new(pop, vgraph, &task->pparams);
	vgraph_delete(vgraph);

	pmemobj_close(pop);
}

/*
 * dump_op -- dump a graph from the pool to a text file
 */
static void
dump_op(struct task *task)
{
	if (task->path == NULL || task->dump == NULL) {
		print_usage();
		exit(EXIT_FAILURE);
	}

	PMEMobjpool *pop = NULL;

	pop = pmemobj_open(task->path, GRAPH_LAYOUT);
	if (pop == NULL) {
		UT_FATAL("!pmemobj_open: %s", task->path);
	}

	struct pgraph_t *pgraph = pgraph_open(pop);

	pgraph_print(pgraph, task->dump);

	pmemobj_close(pop);
}

/*
 * defrag_op -- defragment the pool containing a graph
 */
static void
defrag_op(struct task *task)
{
	if (task->path == NULL) {
		print_usage();
		exit(EXIT_FAILURE);
	}

	PMEMobjpool *pop = NULL;

	pop = pmemobj_open(task->path, GRAPH_LAYOUT);
	if (pop == NULL) {
		UT_FATAL("!pmemobj_open: %s", task->path);
	}

	struct pgraph_t *pgraph = pgraph_open(pop);

	graph_defrag(pop, pgraph);

	pmemobj_close(pop);
}

#define ZERO_ALLOWED (0)
#define ZERO_NOT_ALLOWED (1)

static void
parse_unsigned(unsigned *var, const char *arg, int nonzero)
{
	*var = strtoul(arg, NULL, 10);
	if (*var == ULONG_MAX && errno == ERANGE)
		exit(EXIT_FAILURE);
	if (nonzero && *var == 0)
		exit(EXIT_FAILURE);
}

/*
 * long_options -- command line options
 */
static const struct option long_options[] = {
	{"create",		no_argument,		NULL,	'c'},
	{"dump",		required_argument,	NULL,	'd'},
	{"defrag",		no_argument,		NULL,	'f'},
	{"path",		required_argument,	NULL,	'p'},
	{"seed",		required_argument,	NULL,	's'},
	{"max-nodes",		required_argument,	NULL,	'n'},
	{"max-edges",		required_argument,	NULL,	'e'},
	{"graph-copies",	required_argument,	NULL,	'o'},
	{"help",		no_argument,		NULL,	'h'},
	{NULL,			0,			NULL,	 0 },
};

#define OPT_STR "cdfp::s:n:e:o:h"

/*
 * parse_args -- parse command line arguments
 */
static void
parse_args(int argc, char *argv[], struct task *task)
{
	int opt;
	while ((opt = getopt_long(argc, argv, OPT_STR,
			long_options, NULL)) != -1) {
		switch (opt) {
		case 'c':
				task->op = OP_CREATE;
			break;
		case 'd':
				task->op = OP_DUMP;
				task->dump = optarg;
			break;
		case 'f':
				task->op = OP_DEFRAG;
			break;
		case 'p':
				task->path = optarg;
			break;
		case 's':
			parse_unsigned(&task->seed,
				optarg, ZERO_ALLOWED);
			break;
		case 'n':
			parse_unsigned(&task->vparams.max_nodes,
				optarg, ZERO_NOT_ALLOWED);
			break;
		case 'e':
			parse_unsigned(&task->vparams.max_edges,
				optarg, ZERO_NOT_ALLOWED);
			break;
		case 'o':
			parse_unsigned(&task->pparams.graph_copies,
				optarg, ZERO_NOT_ALLOWED);
			break;
		case 'h':
			print_usage();
			exit(EXIT_SUCCESS);
		default:
			print_usage();
			exit(EXIT_FAILURE);
		}
	}

	if (optind > argc) {
		print_usage();
		exit(EXIT_FAILURE);
	}
}

/*
 * task_init -- initialize task values
 */
static void
task_init(struct task *task)
{
	task->op = OP_MAX;
	task->path = NULL;
	task->dump = NULL;
	task->seed = 0;

	/* good enough defaults - no magic here */
	task->vparams.max_nodes = 50;
	task->vparams.max_edges = 10;
	task->vparams.range_nodes = 10;
	task->vparams.range_edges = 10;
	task->vparams.min_pattern_size = 8;
	task->vparams.max_pattern_size = 1024;

	task->pparams.graph_copies = 10;
}

int
main(int argc, char *argv[])
{
	START(argc, argv, TEST_NAME);

	struct task task;
	task_init(&task);

	parse_args(argc, argv, &task);

	switch (task.op) {
	case OP_CREATE:
		create_op(&task);
		break;
	case OP_DUMP:
		dump_op(&task);
		break;
	case OP_DEFRAG:
		defrag_op(&task);
		break;
	case OP_MAX:
	default:
		print_usage();
		exit(EXIT_FAILURE);
	}

	DONE(NULL);
}

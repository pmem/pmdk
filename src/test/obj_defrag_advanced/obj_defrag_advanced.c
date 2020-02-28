// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/*
 * obj_defrag_advanced.c -- test for libpmemobj defragmentation feature
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stddef.h>
#include <unistd.h>
#include <stdlib.h>

#include "rand.h"
#include "vgraph.h"
#include "pgraph.h"
#include "os_thread.h"
#include "unittest.h"

struct create_params_t {
	uint64_t seed;
	rng_t rng;

	struct vgraph_params vparams;
	struct pgraph_params pparams;
};

/*
 * graph_create -- create a graph
 * - generate an intermediate volatile graph representation
 * - use the volatile graph to allocate a persistent one
 */
static void
graph_create(struct create_params_t *task, PMEMobjpool *pop, PMEMoid *oidp,
		rng_t *rngp)
{
	struct vgraph_t *vgraph = vgraph_new(&task->vparams, rngp);
	pgraph_new(pop, oidp, vgraph, &task->pparams, rngp);
	vgraph_delete(vgraph);
}

/*
 * graph_defrag -- defragment the pool
 * - collect pointers to all PMEMoids
 * - do a sanity checks
 * - call pmemobj_defrag
 * - return # of relocated objects
 */
static size_t
graph_defrag(PMEMobjpool *pop, PMEMoid oid)
{
	struct pgraph_t *pgraph = (struct pgraph_t *)pmemobj_direct(oid);

	/* count number of oids */
	unsigned oidcnt = pgraph->nodes_num;
	for (unsigned i = 0; i < pgraph->nodes_num; ++i) {
		struct pnode_t *pnode = (struct pnode_t *)pmemobj_direct
				(pgraph->nodes[i]);
		oidcnt += pnode->edges_num;
	}

	/* create array of oid pointers */
	PMEMoid **oidv = (PMEMoid **)MALLOC(sizeof(PMEMoid *) * oidcnt);
	unsigned oidi = 0;
	for (unsigned i = 0; i < pgraph->nodes_num; ++i) {
		oidv[oidi++] = &pgraph->nodes[i];

		struct pnode_t *pnode = (struct pnode_t *)pmemobj_direct
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
	UT_ASSERTeq(result.total, pgraph->nodes_num);

	FREE(oidv);

	return result.relocated;
}

/*
 * graph_defrag_ntimes -- defragment the graph N times
 * - where N <= max_rounds
 * - it stops defrag if # of relocated objects == 0
 */
static void
graph_defrag_ntimes(PMEMobjpool *pop, PMEMoid oid, unsigned max_rounds)
{
	size_t relocated;
	unsigned rounds = 0;
	do {
		relocated = graph_defrag(pop, oid);
		++rounds;
	} while (relocated > 0 && rounds < max_rounds);
}

#define HAS_TO_EXIST (1)

/*
 * graph_dump -- dump a graph from the pool to a text file
 */
static void
graph_dump(PMEMoid oid, const char *path, int has_exist)
{
	struct pgraph_t *pgraph = (struct pgraph_t *)pmemobj_direct(oid);
	if (has_exist)
		UT_ASSERTne(pgraph, NULL);

	if (pgraph)
		pgraph_print(pgraph, path);
}

#define FGETS_BUFF_LEN 1024

/*
 * dump_compare -- compare graph dumps
 * Test fails if the contents of dumps do not match
 */
static void
dump_compare(const char *path1, const char *path2)
{
	FILE *dump1 = FOPEN(path1, "r");
	FILE *dump2 = FOPEN(path2, "r");

	char buff1[FGETS_BUFF_LEN];
	char buff2[FGETS_BUFF_LEN];
	char *sret1, *sret2;

	do {
		sret1 = fgets(buff1, FGETS_BUFF_LEN, dump1);
		sret2 = fgets(buff2, FGETS_BUFF_LEN, dump2);

		/* both files have to end at the same time */
		if (!sret1) {
			UT_ASSERTeq(sret2, NULL);

			FCLOSE(dump1);
			FCLOSE(dump2);

			return;
		}

		UT_ASSERTeq(sret1, buff1);
		UT_ASSERTeq(sret2, buff2);

		UT_ASSERTeq(strcmp(buff1, buff2), 0);
	} while (1);
}

/*
 * create_params_init -- initialize create params
 */
static void
create_params_init(struct create_params_t *params)
{
	params->seed = 1;

	/* good enough defaults - no magic here */
	params->vparams.max_nodes = 50;
	params->vparams.max_edges = 10;
	params->vparams.range_nodes = 10;
	params->vparams.range_edges = 10;
	params->vparams.min_pattern_size = 8;
	params->vparams.max_pattern_size = 1024;

	params->pparams.graph_copies = 10;
}

/* global state */
static struct global_t {
	PMEMobjpool *pop;
} global;

/*
 * PMEMobj root object structure
 */
struct root_t {
	unsigned graphs_num;
	PMEMoid graphs[];
};

/*
 * root_size -- calculate a root object size
 */
static inline size_t
root_size(unsigned graph_num, size_t min_root_size)
{
	size_t size = sizeof(struct root_t) + sizeof(PMEMoid) * graph_num;
	return MAX(size, min_root_size);
}

#define QUERY_GRAPHS_NUM UINT_MAX

static struct root_t *
get_root(unsigned graphs_num, size_t min_root_size)
{
	PMEMoid roid;
	struct root_t *root;

	if (graphs_num == QUERY_GRAPHS_NUM) {
		/* allocate a root object without graphs */
		roid = pmemobj_root(global.pop, root_size(0, 0));
		if (OID_IS_NULL(roid))
			UT_FATAL("!pmemobj_root:");
		root = (struct root_t *)pmemobj_direct(roid);
		UT_ASSERTne(root, NULL);

		graphs_num = root->graphs_num;
	}

	UT_ASSERT(graphs_num > 0);

	/* reallocate a root object with all known graphs */
	roid = pmemobj_root(global.pop, root_size(graphs_num, min_root_size));
	if (OID_IS_NULL(roid))
		UT_FATAL("!pmemobj_root:");
	root = (struct root_t *)pmemobj_direct(roid);
	UT_ASSERTne(root, NULL);

	return root;
}

/*
 * parse_nonzero -- parse non-zero unsigned
 */
static void
parse_nonzero(unsigned *var, const char *arg)
{
	unsigned long v = STRTOUL(arg, NULL, 10);
	UT_ASSERTne(v, 0);
	UT_ASSERT(v < UINT_MAX);

	*var = v;
}

#define GRAPH_LAYOUT POBJ_LAYOUT_NAME(graph)

/*
 * op_pool_create -- create a pool
 */
static int
op_pool_create(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: %s <path>", tc->name);

	/* parse arguments */
	const char *path = argv[0];

	/* open a pool */
	global.pop = pmemobj_create(path, GRAPH_LAYOUT, 0, S_IWUSR | S_IRUSR);
	if (global.pop == NULL) {
		UT_FATAL("!pmemobj_create: %s", path);
	}

	return 1;
}

/*
 * op_pool_close -- close the poll
 */
static int
op_pool_close(const struct test_case *tc, int argc, char *argv[])
{
	pmemobj_close(global.pop);
	global.pop = NULL;

	return 0;
}

/*
 * op_graph_create -- create a graph
 */
static int
op_graph_create(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 4)
		UT_FATAL("usage: %s <max-nodes> <max-edges> <graph-copies>"
				" <min-root-size>", tc->name);

	/* parse arguments */
	struct create_params_t cparams;
	create_params_init(&cparams);
	parse_nonzero(&cparams.vparams.max_nodes, argv[0]);
	parse_nonzero(&cparams.vparams.max_edges, argv[1]);
	parse_nonzero(&cparams.pparams.graph_copies, argv[2]);
	size_t min_root_size = STRTOULL(argv[3], NULL, 10);

	struct root_t *root = get_root(1, min_root_size);

	randomize(cparams.seed);

	/* generate a single graph */
	graph_create(&cparams, global.pop, &root->graphs[0], NULL);
	root->graphs_num = 1;
	pmemobj_persist(global.pop, root, root_size(1, min_root_size));

	return 4;
}

/*
 * op_graph_dump -- dump the graph
 */
static int
op_graph_dump(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: %s <dump>", tc->name);

	/* parse arguments */
	const char *dump = argv[0];

	struct root_t *root = get_root(QUERY_GRAPHS_NUM, 0);
	UT_ASSERTeq(root->graphs_num, 1);

	/* dump the graph before defrag */
	graph_dump(root->graphs[0], dump, HAS_TO_EXIST);

	return 1;
}

/*
 * op_graph_defrag -- defrag the graph
 */
static int
op_graph_defrag(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: %s <max-rounds>", tc->name);

	/* parse arguments */
	unsigned max_rounds;
	parse_nonzero(&max_rounds, argv[0]);

	struct root_t *root = get_root(QUERY_GRAPHS_NUM, 0);
	UT_ASSERTeq(root->graphs_num, 1);

	/* do the defrag */
	graph_defrag_ntimes(global.pop, root->graphs[0], max_rounds);

	return 1;
}

/*
 * op_dump_compare -- compare dumps
 */
static int
op_dump_compare(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 2)
		UT_FATAL("usage: %s <dump1> <dump2>", tc->name);

	/* parse arguments */
	const char *dump1 = argv[0];
	const char *dump2 = argv[1];

	dump_compare(dump1, dump2);

	return 2;
}

struct create_n_defrag_params_t {
	unsigned thread_id;

	struct create_params_t cparams;

	PMEMobjpool *pop;
	PMEMoid *oidp;

	unsigned max_rounds;
	unsigned ncycles;
};

/*
 * create_n_defrag_thread -- create and defrag graphs mutiple times
 */
static void *
create_n_defrag_thread(void *arg)
{
	struct create_n_defrag_params_t *params =
			(struct create_n_defrag_params_t *)arg;

	char dump1[PATH_MAX];
	char dump2[PATH_MAX];

	SNPRINTF(dump1, PATH_MAX, "dump_t%u_1.log", params->thread_id);
	SNPRINTF(dump2, PATH_MAX, "dump_t%u_2.log", params->thread_id);

	struct create_params_t *cparams = &params->cparams;

	for (unsigned i = 0; i < params->ncycles; ++i) {
		graph_create(cparams, global.pop, params->oidp, &cparams->rng);
		graph_dump(*params->oidp, dump1, HAS_TO_EXIST);

		graph_defrag_ntimes(params->pop, *params->oidp,
				params->max_rounds);
		graph_dump(*params->oidp, dump2, HAS_TO_EXIST);

		dump_compare(dump1, dump2);

		pgraph_delete(params->oidp);
	}

	return NULL;
}

/*
 * op_graph_create_n_defrag_mt -- multi-threaded graphs creation & defrag
 */
static int
op_graph_create_n_defrag_mt(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 7)
		UT_FATAL("usage: %s <max-nodes> <max-edges> <graph-copies>"
				" <min-root-size> <max-defrag-rounds> <n-threads>"
				"<n-create-defrag-cycles>", tc->name);

	/* parse arguments */
	struct create_params_t cparams;
	create_params_init(&cparams);
	parse_nonzero(&cparams.vparams.max_nodes, argv[0]);
	parse_nonzero(&cparams.vparams.max_edges, argv[1]);
	parse_nonzero(&cparams.pparams.graph_copies, argv[2]);
	size_t min_root_size = STRTOULL(argv[3], NULL, 10);
	unsigned max_rounds;
	parse_nonzero(&max_rounds, argv[4]);
	unsigned nthreads;
	parse_nonzero(&nthreads, argv[5]);
	unsigned ncycles;
	parse_nonzero(&ncycles, argv[6]);

	struct root_t *root = get_root(nthreads, min_root_size);
	root->graphs_num = nthreads;
	pmemobj_persist(global.pop, root, sizeof(*root));

	/* prepare threads params */
	struct create_n_defrag_params_t *paramss =
			(struct create_n_defrag_params_t *)MALLOC(
					sizeof(*paramss) * nthreads);

	for (unsigned i = 0; i < nthreads; ++i) {
		struct create_n_defrag_params_t *params = &paramss[i];

		params->thread_id = i;
		memcpy(&params->cparams, &cparams, sizeof(cparams));
		params->cparams.seed += i;
		randomize_r(&params->cparams.rng, params->cparams.seed);
		params->pop = global.pop;
		params->oidp = &root->graphs[i];
		params->max_rounds = max_rounds;
		params->ncycles = ncycles;
	}

	/* spawn threads */
	os_thread_t *threads = (os_thread_t *)MALLOC(
			sizeof(*threads) * nthreads);
	for (unsigned i = 0; i < nthreads; ++i)
		THREAD_CREATE(&threads[i], NULL, create_n_defrag_thread,
				&paramss[i]);

	/* join all threads */
	void *ret = NULL;
	for (unsigned i = 0; i < nthreads; ++i) {
		THREAD_JOIN(&threads[i], &ret);
		UT_ASSERTeq(ret, NULL);
	}

	FREE(threads);
	FREE(paramss);

	return 7;
}

/*
 * op_pool_open -- open the pool
 */
static int
op_pool_open(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: %s <path>", tc->name);

	/* parse arguments */
	const char *path = argv[0];

	/* open a pool */
	global.pop = pmemobj_open(path, GRAPH_LAYOUT);
	if (global.pop == NULL)
		UT_FATAL("!pmemobj_create: %s", path);

	return 1;
}

/*
 * op_graph_dump_all -- dump all graphs
 */
static int
op_graph_dump_all(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: %s <dump-prefix>", tc->name);

	/* parse arguments */
	const char *dump_prefix = argv[0];

	struct root_t *root = get_root(QUERY_GRAPHS_NUM, 0);

	char dump[PATH_MAX];
	for (unsigned i = 0; i < root->graphs_num; ++i) {
		SNPRINTF(dump, PATH_MAX, "%s_%u.log", dump_prefix, i);
		graph_dump(root->graphs[i], dump, HAS_TO_EXIST);
	}

	return 1;
}

/*
 * ops -- available ops
 */
static struct test_case ops[] = {
	TEST_CASE(op_pool_create),
	TEST_CASE(op_pool_close),
	TEST_CASE(op_graph_create),
	TEST_CASE(op_graph_dump),
	TEST_CASE(op_graph_defrag),
	TEST_CASE(op_dump_compare),
	TEST_CASE(op_graph_create_n_defrag_mt),

	/* for pool validation only */
	TEST_CASE(op_pool_open),
	TEST_CASE(op_graph_dump_all),
};

#define NOPS ARRAY_SIZE(ops)

#define TEST_NAME "obj_defrag_advanced"

int
main(int argc, char *argv[])
{
	START(argc, argv, TEST_NAME);
	TEST_CASE_PROCESS(argc, argv, ops, NOPS);
	DONE(NULL);
}

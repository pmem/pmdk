// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/*
 * pgraph.c -- persistent graph representation
 */

#include <inttypes.h>

#include "unittest.h"

#include "vgraph.h"
#include "pgraph.h"

#define PATTERN 'g'

/*
 * pnode_size -- return the entire of node size
 */
static size_t
pnode_size(unsigned edges_num, size_t pattern_size)
{
	size_t node_size = sizeof(struct pnode_t);
	node_size += sizeof(PMEMoid) * edges_num;
	node_size += pattern_size;
	return node_size;
}

/*
 * pnode_init -- initialize the node
 */
static void
pnode_init(PMEMobjpool *pop, PMEMoid pnode_oid, struct vnode_t *vnode,
		PMEMoid pnodes[])
{
	struct pnode_t *pnode = (struct pnode_t *)pmemobj_direct(pnode_oid);
	pnode->node_id = vnode->node_id;
	pnode->size = vnode->psize;

	/* set edges */
	pnode->edges_num = vnode->edges_num;
	for (unsigned i = 0; i < vnode->edges_num; ++i)
		pnode->edges[i] = pnodes[vnode->edges[i]];

	/* initialize pattern */
	pnode->pattern_size = vnode->pattern_size;
	void *pattern = (void *)&pnode->edges[pnode->edges_num];
	pmemobj_memset(pop, pattern, PATTERN, pnode->pattern_size,
			PMEMOBJ_F_MEM_NOFLUSH);

	/* persist the whole node state */
	pmemobj_persist(pop, (const void *)pnode, pnode->size);
}

/*
 * order_shuffle -- shuffle the nodes in graph
 */
static void
order_shuffle(unsigned *order, unsigned num, rng_t *rngp)
{
	for (unsigned i = 0; i < num; ++i) {
		unsigned j = rand_range(0, num, rngp);
		unsigned temp = order[j];
		order[j] = order[i];
		order[i] = temp;
	}
}

/*
 * order_new -- generate the sequence of the graph nodes allocation
 */
static unsigned *
order_new(struct vgraph_t *vgraph, rng_t *rngp)
{
	unsigned *order = (unsigned *)MALLOC(sizeof(unsigned)
		* vgraph->nodes_num);

	/* initialize id list */
	for (unsigned i = 0; i < vgraph->nodes_num; ++i)
		order[i] = i;

	order_shuffle(order, vgraph->nodes_num, rngp);

	return order;
}

/*
 * pgraph_copy_new -- allocate a persistent copy of the volatile graph
 */
static PMEMoid *
pgraph_copy_new(PMEMobjpool *pop, struct vgraph_t *vgraph, rng_t *rngp)
{
	/* to be returned array of PMEMoids to raw nodes allocations */
	PMEMoid *nodes = (PMEMoid *)MALLOC(sizeof(PMEMoid) * vgraph->nodes_num);

	/* generates random order of nodes allocation */
	unsigned *order = order_new(vgraph, rngp);

	/* allocate the nodes in the random order */
	int ret;
	for (unsigned i = 0; i < vgraph->nodes_num; ++i) {
		struct vnode_t vnode = vgraph->node[order[i]];
		PMEMoid *node = &nodes[order[i]];
		ret = pmemobj_alloc(pop, node, vnode.psize, 0, NULL, NULL);
		UT_ASSERTeq(ret, 0);
	}

	FREE(order);

	return nodes;
}

/*
 * pgraph_copy_delete -- free copies of the graph
 */
static void
pgraph_copy_delete(PMEMoid *nodes, unsigned num)
{
	for (unsigned i = 0; i < num; ++i) {
		if (OID_IS_NULL(nodes[i]))
			continue;

		pmemobj_free(&nodes[i]);
	}

	FREE(nodes);
}

/*
 * pgraph_size -- return the struct pgraph_t size
 */
static size_t
pgraph_size(unsigned nodes_num)
{
	return sizeof(struct pgraph_t) + sizeof(PMEMoid) * nodes_num;
}

/*
 * pgraph_new -- allocate a new persistent graph in such a way
 * that the fragmentation is as large as possible
 */
void
pgraph_new(PMEMobjpool *pop, PMEMoid *oidp, struct vgraph_t *vgraph,
		struct pgraph_params *params, rng_t *rngp)
{
	int ret = pmemobj_alloc(pop, oidp, pgraph_size(vgraph->nodes_num),
			0, NULL, NULL);
	UT_ASSERTeq(ret, 0);

	struct pgraph_t *pgraph = (struct pgraph_t *)pmemobj_direct(*oidp);
	pgraph->nodes_num = vgraph->nodes_num;
	pmemobj_persist(pop, pgraph, sizeof(*pgraph));

	/* calculate size of pnodes */
	for (unsigned i = 0; i < vgraph->nodes_num; ++i) {
		struct vnode_t *vnode = &vgraph->node[i];
		vnode->psize = pnode_size(vnode->edges_num,
				vnode->pattern_size);
	}

	/* prepare multiple copies of the nodes */
	unsigned copies_num = rand_range(1, params->graph_copies, rngp);
	PMEMoid **copies = (PMEMoid **)MALLOC(sizeof(PMEMoid *) * copies_num);
	for (unsigned i = 0; i < copies_num; ++i)
		copies[i] = pgraph_copy_new(pop, vgraph, rngp);

	/* peek exactly the one copy of each node */
	for (unsigned i = 0; i < pgraph->nodes_num; ++i) {
		unsigned copy_id = rand_range(0, copies_num, rngp);
		pgraph->nodes[i] = copies[copy_id][i];
		copies[copy_id][i] = OID_NULL;
	}
	pmemobj_persist(pop, pgraph->nodes,
			sizeof(PMEMoid) * pgraph->nodes_num);

	/* free unused copies of the nodes */
	for (unsigned i = 0; i < copies_num; ++i)
		pgraph_copy_delete(copies[i], vgraph->nodes_num);

	FREE(copies);

	/* initialize pnodes */
	for (unsigned i = 0; i < pgraph->nodes_num; ++i)
		pnode_init(pop, pgraph->nodes[i], &vgraph->node[i],
				pgraph->nodes);
}

/*
 * pgraph_delete -- free the persistent graph
 */
void
pgraph_delete(PMEMoid *oidp)
{
	struct pgraph_t *pgraph = (struct pgraph_t *)pmemobj_direct(*oidp);

	/* free pnodes */
	for (unsigned i = 0; i < pgraph->nodes_num; ++i)
		pmemobj_free(&pgraph->nodes[i]);

	pmemobj_free(oidp);
}

/*
 * pgraph_print --  print graph in human readable format
 */
void
pgraph_print(struct pgraph_t *pgraph, const char *dump)
{
	UT_ASSERTne(dump, NULL);

	FILE *out = FOPEN(dump, "w");

	/* print the graph statistics */
	fprintf(out, "# of nodes: %u\n", pgraph->nodes_num);

	uint64_t total_edges_num = 0;
	for (unsigned i = 0; i < pgraph->nodes_num; ++i) {
		PMEMoid node_oid = pgraph->nodes[i];
		struct pnode_t *pnode =
				(struct pnode_t *)pmemobj_direct(node_oid);
		total_edges_num += pnode->edges_num;
	}
	fprintf(out, "Total # of edges: %" PRIu64 "\n\n", total_edges_num);

	/* print the graph itself */
	for (unsigned i = 0; i < pgraph->nodes_num; ++i) {
		PMEMoid node_oid = pgraph->nodes[i];
		struct pnode_t *pnode =
				(struct pnode_t *)pmemobj_direct(node_oid);
		fprintf(out, "%u:", pnode->node_id);
		for (unsigned j = 0; j < pnode->edges_num; ++j) {
			PMEMoid edge_oid = pnode->edges[j];
			struct pnode_t *edge =
				(struct pnode_t *)pmemobj_direct(edge_oid);
			UT_ASSERT(edge->node_id < pgraph->nodes_num);
			fprintf(out, "%u, ", edge->node_id);
		}
		fprintf(out, "\n");
	}

	FCLOSE(out);
}

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
 * pgraph.c -- persistent graph representation
 */

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
	size_t node_size = sizeof(struct pnode);
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
	struct pnode *pnode = (struct pnode *)pmemobj_direct(pnode_oid);
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
order_shuffle(unsigned *order, unsigned num)
{
	for (unsigned i = 0; i < num; ++i) {
		unsigned j = rand_range(0, num);
		unsigned temp = order[j];
		order[j] = order[i];
		order[i] = temp;
	}
}

/*
 * order_new -- generate the sequence of the graph nodes allocation
 */
static unsigned *
order_new(struct vgraph_t *vgraph)
{
	unsigned *order = (unsigned *)malloc(sizeof(unsigned)
		* vgraph->nodes_num);

	/* initialize id list */
	for (unsigned i = 0; i < vgraph->nodes_num; ++i)
		order[i] = i;

	order_shuffle(order, vgraph->nodes_num);

	return order;
}

/*
 * pgraph_copy_new -- allocate a persistent copy of the volatile graph
 */
static PMEMoid *
pgraph_copy_new(PMEMobjpool *pop, struct vgraph_t *vgraph)
{
	/* to be returned array of PMEMoids to raw nodes allocations */
	PMEMoid *nodes = (PMEMoid *)malloc(sizeof(PMEMoid) * vgraph->nodes_num);

	/* generates random order of nodes allocation */
	unsigned *order = order_new(vgraph);

	/* allocate the nodes in the random order */
	int ret;
	for (unsigned i = 0; i < vgraph->nodes_num; ++i) {
		struct vnode_t vnode = vgraph->node[order[i]];
		PMEMoid *node = &nodes[order[i]];
		ret = pmemobj_alloc(pop, node, vnode.psize, 0, NULL, NULL);
		UT_ASSERTeq(ret, 0);
	}

	free(order);

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

	free(nodes);
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
struct pgraph_t *
pgraph_new(PMEMobjpool *pop, struct vgraph_t *vgraph,
		struct pgraph_params *params)
{
	size_t root_size = pgraph_size(vgraph->nodes_num);
	PMEMoid root_oid = pmemobj_root(pop, root_size);
	if (OID_IS_NULL(root_oid))
		UT_FATAL("!pmemobj_root:");

	struct pgraph_t *pgraph = (struct pgraph_t *)pmemobj_direct(root_oid);
	pgraph->nodes_num = vgraph->nodes_num;

	/* calculate size of pnodes */
	for (unsigned i = 0; i < vgraph->nodes_num; ++i) {
		struct vnode_t *vnode = &vgraph->node[i];
		vnode->psize = pnode_size(vnode->edges_num,
				vnode->pattern_size);
	}

	/* prepare multiple copies of the nodes */
	unsigned copies_num = rand_range(1, params->graph_copies);
	PMEMoid **copies = (PMEMoid **)malloc(sizeof(PMEMoid *) * copies_num);
	for (unsigned i = 0; i < copies_num; ++i)
		copies[i] = pgraph_copy_new(pop, vgraph);

	/* peek exactly the one copy of each node */
	for (unsigned i = 0; i < pgraph->nodes_num; ++i) {
		unsigned copy_id = rand_range(0, copies_num);
		pgraph->nodes[i] = copies[copy_id][i];
		copies[copy_id][i] = OID_NULL;
	}

	/* free unused copies of the nodes */
	for (unsigned i = 0; i < copies_num; ++i)
		pgraph_copy_delete(copies[i], vgraph->nodes_num);

	free(copies);

	/* initialize pnodes */
	for (unsigned i = 0; i < pgraph->nodes_num; ++i)
		pnode_init(pop, pgraph->nodes[i], &vgraph->node[i],
				pgraph->nodes);

	return pgraph;
}

/*
 * pgraph_open -- open existing graph
 */
struct pgraph_t *
pgraph_open(PMEMobjpool *pop)
{
	PMEMoid root_oid = pmemobj_root(pop, sizeof(struct pgraph_t));
	if (OID_IS_NULL(root_oid))
		UT_FATAL("!pmemobj_root:");

	struct pgraph_t *pgraph = (struct pgraph_t *)pmemobj_direct(root_oid);
	size_t root_size = pgraph_size(pgraph->nodes_num);
	root_oid = pmemobj_root(pop, root_size);
	if (OID_IS_NULL(root_oid))
		UT_FATAL("!pmemobj_root:");

	pgraph = (struct pgraph_t *)pmemobj_direct(root_oid);

	return pgraph;
}

/*
 * pgraph_print --  print graph in human readable format
 */
void
pgraph_print(struct pgraph_t *pgraph, const char *dump)
{
	UT_ASSERTne(dump, NULL);

	FILE *out = FOPEN(dump, "w");

	for (unsigned i = 0; i < pgraph->nodes_num; ++i) {
		PMEMoid node_oid = pgraph->nodes[i];
		struct pnode *pnode = (struct pnode *)pmemobj_direct(node_oid);
		fprintf(out, "%u:", pnode->node_id);
		for (unsigned j = 0; j < pnode->edges_num; ++j) {
			PMEMoid edge_oid = pnode->edges[j];
			struct pnode *edge =
				(struct pnode *)pmemobj_direct(edge_oid);
			fprintf(out, "%u, ", edge->node_id);
		}
		fprintf(out, "\n");
	}

	FCLOSE(out);
}

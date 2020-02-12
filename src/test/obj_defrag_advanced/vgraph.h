// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/*
 * vgraph.h -- volatile graph representation
 */

#ifndef OBJ_DEFRAG_ADV_VGRAPH
#define OBJ_DEFRAG_ADV_VGRAPH

#include "rand.h"

struct vgraph_params
{
	unsigned max_nodes; /* max # of nodes per graph */
	unsigned max_edges; /* max # of edges per node */
	/* # of nodes is between [max_nodes - range_nodes, max_nodes] */
	unsigned range_nodes;
	/* # of edges is between [max_edges - range_edges, max_edges] */
	unsigned range_edges;
	unsigned min_pattern_size;
	unsigned max_pattern_size;
};

struct vnode_t
{
	unsigned node_id;
	unsigned edges_num; /* # of edges starting from this node */
	unsigned *edges; /* ids of nodes the edges are pointing to */

	/* the persistent node attributes */
	size_t pattern_size; /* size of the pattern allocated after the node */
	size_t psize; /* the total size of the node */
};

struct vgraph_t
{
	unsigned nodes_num;
	struct vnode_t node[];
};

unsigned rand_range(unsigned min, unsigned max, rng_t *rngp);

struct vgraph_t *vgraph_new(struct vgraph_params *params, rng_t *rngp);
void vgraph_delete(struct vgraph_t *graph);

#endif

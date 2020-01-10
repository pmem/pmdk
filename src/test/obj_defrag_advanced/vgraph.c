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
 * vgraph.c -- volatile graph representation
 */

#include <stdlib.h>
#include <stdio.h>

#include "rand.h"
#include "unittest.h"
#include "vgraph.h"

/*
 * rand_range -- generate pseudo-random number from given interval [min, max]
 */
unsigned
rand_range(unsigned min, unsigned max, rng_t *rngp)
{
	if (min == max)
		return min;

	if (min > max)
		UT_FATAL("!rand_range");

	unsigned ret;
	if (rngp)
		ret = (unsigned)rnd64_r(rngp);
	else
		ret = (unsigned)rnd64();

	return ((unsigned)ret % (max - min)) + min;
}

/*
 * vnode_new -- allocate a new volatile node
 */
static void
vnode_new(struct vnode_t *node, unsigned v, struct vgraph_params *params,
		rng_t *rngp)
{
	unsigned min_edges = 1;
	if (params->max_edges > params->range_edges)
		min_edges = params->max_edges - params->range_edges;
	unsigned edges_num = rand_range(min_edges, params->max_edges, rngp);
	node->node_id = v;
	node->edges_num = edges_num;
	node->edges = (unsigned *)MALLOC(sizeof(int) * edges_num);
	node->pattern_size = rand_range(params->min_pattern_size,
			params->max_pattern_size, rngp);
}

/*
 * vnode_delete -- free a volatile node
 */
static void
vnode_delete(struct vnode_t *node)
{
	FREE(node->edges);
}

/*
 * vgraph_get_node -- return node in graph based on given id_node
 */
static struct vnode_t *
vgraph_get_node(struct vgraph_t *graph, unsigned id_node)
{
	struct vnode_t *node;

	node = &graph->node[id_node];
	return node;
}

/*
 * vgraph_add_edges -- randomly assign destination nodes to the edges
 */
static void
vgraph_add_edges(struct vgraph_t *graph, rng_t *rngp)
{
	unsigned nodes_count = 0;
	unsigned edges_count = 0;
	struct vnode_t *node;
	for (nodes_count = 0; nodes_count < graph->nodes_num; nodes_count++) {
		node = vgraph_get_node(graph, nodes_count);
		unsigned edges_num = node->edges_num;
		for (edges_count = 0; edges_count < edges_num; edges_count++) {
			unsigned node_link =
					rand_range(0, graph->nodes_num, rngp);
			node->edges[edges_count] = node_link;
		}
	}
}

/*
 * vgraph_new -- allocate a new volatile graph
 */
struct vgraph_t *
vgraph_new(struct vgraph_params *params, rng_t *rngp)
{
	unsigned min_nodes = 1;
	if (params->max_nodes > params->range_nodes)
		min_nodes = params->max_nodes - params->range_nodes;
	unsigned nodes_num = rand_range(min_nodes, params->max_nodes, rngp);

	struct vgraph_t *graph =
			(struct vgraph_t *)MALLOC(sizeof(struct vgraph_t) +
					sizeof(struct vnode_t) * nodes_num);
	graph->nodes_num = nodes_num;

	for (unsigned i = 0; i < nodes_num; i++) {
		vnode_new(&graph->node[i], i, params, rngp);
	}

	vgraph_add_edges(graph, rngp);

	return graph;
}

/*
 * vgraph_delete -- free the volatile graph
 */
void
vgraph_delete(struct vgraph_t *graph)
{
	for (unsigned i = 0; i < graph->nodes_num; i++)
		vnode_delete(&graph->node[i]);

	FREE(graph);
}

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

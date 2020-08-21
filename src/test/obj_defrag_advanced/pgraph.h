/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2020, Intel Corporation */

/*
 * pgraph.h -- persistent graph representation
 */

#ifndef OBJ_DEFRAG_ADV_PGRAPH
#define OBJ_DEFRAG_ADV_PGRAPH

#include <libpmemobj/base.h>

struct pgraph_params
{
	unsigned graph_copies;
};

struct pnode_t
{
	unsigned node_id;
	unsigned edges_num;
	size_t pattern_size;
	size_t size;
	PMEMoid edges[];
};

struct pgraph_t
{
	unsigned nodes_num;
	PMEMoid nodes[];
};

void pgraph_new(PMEMobjpool *pop, PMEMoid *oidp, struct vgraph_t *vgraph,
		struct pgraph_params *params, rng_t *rngp);
void pgraph_delete(PMEMoid *oidp);

void pgraph_print(struct pgraph_t *graph, const char *dump);

#endif /* pgraph.h */

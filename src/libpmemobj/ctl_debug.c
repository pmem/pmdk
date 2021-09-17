// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2018-2021, Intel Corporation */

/*
 * ctl_debug.c -- implementation of the debug CTL namespace
 */

#include "ctl.h"
#include "ctl_debug.h"
#include "obj.h"

/*
 * CTL_WRITE_HANDLER(alloc_pattern) -- sets the alloc_pattern field in heap
 */
static int
CTL_WRITE_HANDLER(alloc_pattern)(void *ctx,
	enum ctl_query_source source, void *arg, struct ctl_indexes *indexes)
{
	/* suppress unused-parameter errors */
	SUPPRESS_UNUSED(source, indexes);

	PMEMobjpool *pop = ctx;
	int arg_in = *(int *)arg;

	pop->heap.alloc_pattern = arg_in;
	return 0;
}

/*
 * CTL_READ_HANDLER(alloc_pattern) -- returns alloc_pattern heap field
 */
static int
CTL_READ_HANDLER(alloc_pattern)(void *ctx,
	enum ctl_query_source source, void *arg, struct ctl_indexes *indexes)
{
	/* suppress unused-parameter errors */
	SUPPRESS_UNUSED(source, indexes);

	PMEMobjpool *pop = ctx;
	int *arg_out = arg;

	*arg_out = pop->heap.alloc_pattern;
	return 0;
}

static const struct ctl_argument CTL_ARG(alloc_pattern) = CTL_ARG_LONG_LONG;

static const struct ctl_node CTL_NODE(heap)[] = {
	CTL_LEAF_RW(alloc_pattern),

	CTL_NODE_END
};

static const struct ctl_node CTL_NODE(debug)[] = {
	CTL_CHILD(heap),

	CTL_NODE_END
};

/*
 * debug_ctl_register -- registers ctl nodes for "debug" module
 */
void
debug_ctl_register(PMEMobjpool *pop)
{
	CTL_REGISTER_MODULE(pop->ctl, debug);
}

/*
 * Copyright 2018-2019, Intel Corporation
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

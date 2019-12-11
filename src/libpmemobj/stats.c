/*
 * Copyright 2017-2019, Intel Corporation
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
 * stats.c -- implementation of statistics
 */

#include "obj.h"
#include "stats.h"

STATS_CTL_HANDLER(persistent, curr_allocated, heap_curr_allocated);

STATS_CTL_HANDLER(transient, run_allocated, heap_run_allocated);
STATS_CTL_HANDLER(transient, run_active, heap_run_active);

static const struct ctl_node CTL_NODE(heap)[] = {
	STATS_CTL_LEAF(persistent, curr_allocated),
	STATS_CTL_LEAF(transient, run_allocated),
	STATS_CTL_LEAF(transient, run_active),

	CTL_NODE_END
};

/*
 * CTL_READ_HANDLER(enabled) -- returns whether or not statistics are enabled
 */
static int
CTL_READ_HANDLER(enabled)(void *ctx,
	enum ctl_query_source source, void *arg,
	struct ctl_indexes *indexes)
{
	PMEMobjpool *pop = ctx;

	int *arg_out = arg;

	*arg_out = pop->stats->enabled > 0;

	return 0;
}

/*
 * CTL_WRITE_HANDLER(enabled) -- enables or disables statistics counting
 */
static int
CTL_WRITE_HANDLER(enabled)(void *ctx,
	enum ctl_query_source source, void *arg,
	struct ctl_indexes *indexes)
{
	PMEMobjpool *pop = ctx;

	int arg_in = *(int *)arg;

	pop->stats->enabled = arg_in > 0;

	return 0;
}

static const struct ctl_argument CTL_ARG(enabled) = CTL_ARG_BOOLEAN;

static const struct ctl_node CTL_NODE(stats)[] = {
	CTL_CHILD(heap),
	CTL_LEAF_RW(enabled),

	CTL_NODE_END
};

/*
 * stats_new -- allocates and initializes statistics instance
 */
struct stats *
stats_new(PMEMobjpool *pop)
{
	struct stats *s = Malloc(sizeof(*s));
	if (s == NULL) {
		ERR("!Malloc");
		return NULL;
	}

	s->enabled = 0;
	s->persistent = &pop->stats_persistent;
	VALGRIND_ADD_TO_GLOBAL_TX_IGNORE(s->persistent, sizeof(*s->persistent));
	s->transient = Zalloc(sizeof(struct stats_transient));
	if (s->transient == NULL)
		goto error_transient_alloc;

	return s;

error_transient_alloc:
	Free(s);
	return NULL;
}

/*
 * stats_delete -- deletes statistics instance
 */
void
stats_delete(PMEMobjpool *pop, struct stats *s)
{
	pmemops_persist(&pop->p_ops, s->persistent,
	sizeof(struct stats_persistent));
	Free(s->transient);
	Free(s);
}

/*
 * stats_ctl_register -- registers ctl nodes for statistics
 */
void
stats_ctl_register(PMEMobjpool *pop)
{
	CTL_REGISTER_MODULE(pop->ctl, stats);
}

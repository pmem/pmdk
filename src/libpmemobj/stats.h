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
 * stats.h -- definitions of statistics
 */

#ifndef LIBPMEMOBJ_STATS_H
#define LIBPMEMOBJ_STATS_H 1

#include "ctl.h"

#ifdef __cplusplus
extern "C" {
#endif

struct stats_transient {
	uint64_t heap_run_allocated;
	uint64_t heap_run_active;
};

struct stats_persistent {
	uint64_t heap_curr_allocated;
};

struct stats {
	int enabled;
	struct stats_transient *transient;
	struct stats_persistent *persistent;
};

#define STATS_INC(stats, type, name, value) do {\
	if ((stats)->enabled)\
		util_fetch_and_add64((&(stats)->type->name), (value));\
} while (0)

#define STATS_SUB(stats, type, name, value) do {\
	if ((stats)->enabled)\
		util_fetch_and_sub64((&(stats)->type->name), (value));\
} while (0)

#define STATS_SET(stats, type, name, value) do {\
	if ((stats)->enabled)\
		util_atomic_store_explicit64((&(stats)->type->name), (value),\
		memory_order_release);\
} while (0)

#define STATS_CTL_LEAF(type, name)\
{CTL_STR(name), CTL_NODE_LEAF,\
{CTL_READ_HANDLER(type##_##name), NULL, NULL},\
NULL, NULL}

#define STATS_CTL_HANDLER(type, name, varname)\
static int CTL_READ_HANDLER(type##_##name)(void *ctx,\
	enum ctl_query_source source, void *arg, struct ctl_indexes *indexes)\
{\
	PMEMobjpool *pop = ctx;\
	uint64_t *argv = arg;\
	util_atomic_load_explicit64(&pop->stats->type->varname,\
		argv, memory_order_acquire);\
	return 0;\
}

void stats_ctl_register(PMEMobjpool *pop);

struct stats *stats_new(PMEMobjpool *pop);
void stats_delete(PMEMobjpool *pop, struct stats *stats);

#ifdef __cplusplus
}
#endif

#endif

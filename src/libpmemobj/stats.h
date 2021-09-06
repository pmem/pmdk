/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2017-2020, Intel Corporation */

/*
 * stats.h -- definitions of statistics
 */

#ifndef LIBPMEMOBJ_STATS_H
#define LIBPMEMOBJ_STATS_H 1

#include "ctl.h"
#include "libpmemobj/ctl.h"

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
	enum pobj_stats_enabled enabled;
	struct stats_transient *transient;
	struct stats_persistent *persistent;
};

#define STATS_INC(stats, type, name, value) do {\
	STATS_INC_##type(stats, name, value);\
} while (0)

#define STATS_INC_transient(stats, name, value) do {\
	if ((stats)->enabled == POBJ_STATS_ENABLED_TRANSIENT ||\
	(stats)->enabled == POBJ_STATS_ENABLED_BOTH)\
		util_fetch_and_add64((&(stats)->transient->name), (value));\
} while (0)

#define STATS_INC_persistent(stats, name, value) do {\
	if ((stats)->enabled == POBJ_STATS_ENABLED_PERSISTENT ||\
	(stats)->enabled == POBJ_STATS_ENABLED_BOTH)\
		util_fetch_and_add64((&(stats)->persistent->name), (value));\
} while (0)

#define STATS_SUB(stats, type, name, value) do {\
	STATS_SUB_##type(stats, name, value);\
} while (0)

#define STATS_SUB_transient(stats, name, value) do {\
	if ((stats)->enabled == POBJ_STATS_ENABLED_TRANSIENT ||\
	(stats)->enabled == POBJ_STATS_ENABLED_BOTH)\
		util_fetch_and_sub64((&(stats)->transient->name), (value));\
} while (0)

#define STATS_SUB_persistent(stats, name, value) do {\
	if ((stats)->enabled == POBJ_STATS_ENABLED_PERSISTENT ||\
	(stats)->enabled == POBJ_STATS_ENABLED_BOTH)\
		util_fetch_and_sub64((&(stats)->persistent->name), (value));\
} while (0)

#define STATS_SET(stats, type, name, value) do {\
	STATS_SET_##type(stats, name, value);\
} while (0)

#define STATS_SET_transient(stats, name, value) do {\
	if ((stats)->enabled == POBJ_STATS_ENABLED_TRANSIENT ||\
	(stats)->enabled == POBJ_STATS_ENABLED_BOTH)\
		util_atomic_store_explicit64((&(stats)->transient->name),\
		(value), memory_order_release);\
} while (0)

#define STATS_SET_persistent(stats, name, value) do {\
	if ((stats)->enabled == POBJ_STATS_ENABLED_PERSISTENT ||\
	(stats)->enabled == POBJ_STATS_ENABLED_BOTH)\
		util_atomic_store_explicit64((&(stats)->persistent->name),\
		(value), memory_order_release);\
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

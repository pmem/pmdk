/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2022-2023, Intel Corporation */

/*
 * libpmem2/async.h -- definitions of libpmem2 functions and structs for
 * asynchronous operations
 *
 * See pmem2_async(3) for details.
 */

#ifndef LIBPMEM2_ASYNC
#define LIBPMEM2_ASYNC 1

#include <libpmem2/base.h>
#include <libminiasync/vdm.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ASYNC_DEPR_STR "Async support for libpmem2 is deprecated."
#ifdef _WIN32
#define ASYNC_DEPR_ATTR __declspec(deprecated(ASYNC_DEPR_STR))
#define WIN_DEPR_STR "Windows support is deprecated."
#define WIN_DEPR_ATTR __declspec(deprecated(WIN_DEPR_STR))
#else
#define ASYNC_DEPR_ATTR __attribute__((deprecated(ASYNC_DEPR_STR)))
#endif

ASYNC_DEPR_ATTR
int pmem2_config_set_vdm(struct pmem2_config *cfg, struct vdm *vdm);

/*
 * Structures needed for persist future
 */
struct pmem2_persist_future_data {
	struct pmem2_map *map;
	void *ptr;
	size_t size;
};

struct pmem2_persist_future_output {
	/*
	 * Windows C compiler does not accept empty structs
	 */
	uint64_t unused;
};

/*
 * Declaration of pmem2_persist_future struct
 */
FUTURE(pmem2_persist_future,
	struct pmem2_persist_future_data,
	struct pmem2_persist_future_output);

/*
 * Implementation of persist future, called upon future_poll
 */
#ifdef _WIN32
WIN_DEPR_ATTR
#endif
static inline enum future_state
pmem2_persist_future_impl(struct future_context *ctx,
	struct future_notifier *notifier)
{
	if (notifier) notifier->notifier_used = FUTURE_NOTIFIER_NONE;

	struct pmem2_persist_future_data *data =
		future_context_get_data(ctx);
	pmem2_persist_fn persist =
		pmem2_get_persist_fn(data->map);
	persist(data->ptr, data->size);
	return FUTURE_STATE_COMPLETE;
}

/*
 * Union containing all possible futures called after vdm operation future
 * in pmem2_future
 */
union pmem2_finalize_future {
	struct pmem2_persist_future persist;
	struct { char data[64]; } pad;
};

/*
 * Returns a future for persisting data
 */
#ifdef _WIN32
WIN_DEPR_ATTR
#endif
static inline union pmem2_finalize_future
pmem2_persist_future(struct pmem2_map *map, void *ptr, size_t size)
{
	union pmem2_finalize_future future;
	future.persist.data.map = map;
	future.persist.data.ptr = ptr;
	future.persist.data.size = size;

	FUTURE_INIT(&future.persist, pmem2_persist_future_impl);

	return future;
}

/*
 * Data for chain pmem2_future which contains vdm operation
 * future and future for finalizing the operation for e.g. persisting data.
 */
struct pmem2_future_data {
	FUTURE_CHAIN_ENTRY(struct vdm_operation_future, op);
	FUTURE_CHAIN_ENTRY_LAST(union pmem2_finalize_future, fin);
};

struct pmem2_future_output {
	void *dest;
};

/*
 * Declaration of struct pmem2_future
 */
FUTURE(pmem2_future, struct pmem2_future_data,
	struct pmem2_future_output);

ASYNC_DEPR_ATTR
struct pmem2_future pmem2_memcpy_async(struct pmem2_map *map,
	void *pmemdest, const void *src, size_t len, unsigned flags);

ASYNC_DEPR_ATTR
struct pmem2_future pmem2_memmove_async(struct pmem2_map *map,
	void *pmemdest, const void *src, size_t len, unsigned flags);

ASYNC_DEPR_ATTR
struct pmem2_future pmem2_memset_async(struct pmem2_map *map,
	void *str, int c, size_t n, unsigned flags);

#ifdef __cplusplus
}
#endif

#endif /* libpmem2/async.h */

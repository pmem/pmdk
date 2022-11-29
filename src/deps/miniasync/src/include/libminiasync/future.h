/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2021-2022, Intel Corporation */

/*
 * future.h - public definitions for the future type, its associated state and
 * related functionality.
 *
 * A future is an abstract type representing a task, or a collection of tasks,
 * that can be executed incrementally by polling until the operation
 * is complete. Futures are typically meant to be implemented by library
 * developers and then used by applications to concurrently run multiple,
 * possibly unrelated, tasks.
 *
 * A future contains the following context:
 * - current state of execution for the future
 * - a function pointer for the task
 * - structure for data which is the required state needed to perform the task
 * - structure for output to store the result of the task
 * - the size of the data and output structures (both can be 0)
 *
 * A future definition must begin an instance of the `struct future` type, which
 * contains all common metadata for all futures, followed by the structures for
 * data and output. The library provides convenience macros to simplify
 * the definition of user-defined future types.
 *
 * Applications must call the `future_poll` method to make progress on the task
 * associated with the future. This function will perform
 * an implementation-defined operation towards completing the task and return
 * the future's current state. Futures are generally safe to poll until they
 * are complete. Unless the documentation for a specific future implementation
 * indicates otherwise, futures can be moved in memory and don't always have
 * to be polled by the same thread.
 *
 * Optionally, future implementations can accept wakers for use in polling.
 * A future can use a waker to signal the caller that some progress can be made
 * and the future should be polled again. This is useful to avoid busy polling
 * when the future is waiting for some asynchronous operation to finish
 * or for some resource to become available.
 *
 * A waker is a tuple composed of a function pointer and a data context pointer.
 * If a waker is supplied and consumed by a future, it will call the provided
 * function with its data pointer. The caller needs to make sure that the waker
 * is safe to call until the future is complete or until it supplies a different
 * waker to the poll method.
 * The waker implementation needs to be thread-safe.
 *
 * TODO: wakers are not optional right now. And are kind of limited. Futures
 * should have a way of indicating to the caller that it was consumed.
 * Maybe a pointer to a tagged union?
 *
 * To facilitate the composition of futures, the library provides a generic
 * "chain" future implementation that applications can use to compose
 * two or more futures into a chain on sequentially executed tasks. The futures
 * that make up a chain are executed in the order in which they are stored
 * in memory. To share state between the futures, each chain entry contains
 * a `map` function to map the state of the just-completed future onto the next
 * future state. Applications can use the map function of the last future in
 * a chain to map its output to the output of the entire chain.
 */

#ifndef FUTURE_H
#define FUTURE_H 1

#include <stddef.h>
#include <stdint.h>

#if defined(__x86_64__) || defined(__amd64__) || defined(_M_X64) || \
	defined(_M_AMD64)
#include <emmintrin.h>

#define __FUTURE_WAIT() _mm_pause()
#else
#include <sched.h>
#define __FUTURE_WAIT() sched_yield()
#endif

#ifdef __cplusplus
extern "C" {
#endif

enum future_state {
	FUTURE_STATE_IDLE,
	FUTURE_STATE_COMPLETE,
	FUTURE_STATE_RUNNING,
};

struct future_context {
	size_t data_size;
	size_t output_size;
	enum future_state state;
	uint32_t padding;
};

typedef void (*future_waker_wake_fn)(void *data);

struct future_waker {
	void *data;
	future_waker_wake_fn wake;
};

struct future_poller {
	uint64_t *ptr_to_monitor;
};

enum future_notifier_type {
	FUTURE_NOTIFIER_NONE,
	FUTURE_NOTIFIER_WAKER,
	FUTURE_NOTIFIER_POLLER,
};

struct future_notifier {
	struct future_waker waker;
	struct future_poller poller;
	enum future_notifier_type notifier_used;
	uint32_t padding;
};

enum future_property {
	FUTURE_PROPERTY_ASYNC,
};

static inline void *
future_context_get_data(struct future_context *context)
{
	return (char *)context + sizeof(struct future_context);
}

static inline void *
future_context_get_output(struct future_context *context)
{
	return (char *)future_context_get_data(context) + context->data_size;
}

static inline size_t
future_context_get_size(struct future_context *context)
{
	return context->data_size + context->output_size;
}

#define FUTURE_WAKER_WAKE(_wakerp)\
((_wakerp)->wake((_wakerp)->data))

typedef enum future_state (*future_task_fn)(struct future_context *context,
			struct future_notifier *notifier);
typedef int (*future_has_property_fn)(void *future,
			enum future_property property);

struct future {
	future_task_fn task;
	future_has_property_fn has_property;
	struct future_context context;
};

#define FUTURE(_name, _data_type, _output_type)\
	struct _name {\
		struct future base;\
		_data_type data;\
		_output_type output;\
	}

#define FUTURE_INIT_EXT(_futurep, _taskfn, _propertyfn)\
do {\
	(_futurep)->base.task = (_taskfn);\
	(_futurep)->base.has_property = (_propertyfn);\
	(_futurep)->base.context.state = (FUTURE_STATE_IDLE);\
	(_futurep)->base.context.data_size = sizeof((_futurep)->data);\
	(_futurep)->base.context.output_size =\
		sizeof((_futurep)->output);\
	(_futurep)->base.context.padding = 0;\
} while (0)

#define FUTURE_INIT(_futurep, _taskfn)\
FUTURE_INIT_EXT((_futurep), (_taskfn), future_has_property_default)

#define FUTURE_INIT_COMPLETE(_futurep)\
do {\
	(_futurep)->base.task = NULL;\
	(_futurep)->base.has_property = NULL;\
	(_futurep)->base.context.state = (FUTURE_STATE_COMPLETE);\
	(_futurep)->base.context.data_size = sizeof((_futurep)->data);\
	(_futurep)->base.context.output_size =\
		sizeof((_futurep)->output);\
	(_futurep)->base.context.padding = 0;\
} while (0)

#define FUTURE_AS_RUNNABLE(futurep) (&(futurep)->base)
#define FUTURE_OUTPUT(futurep) (&(futurep)->output)
#define FUTURE_DATA(futurep) (&(futurep)->data)
#define FUTURE_STATE(futurep) ((futurep)->base.context.state)

typedef void (*future_map_fn)(struct future_context *lhs,
			struct future_context *rhs, void *arg);
typedef void (*future_init_fn)(void *future,
			struct future_context *chain_fut, void *arg);

struct future_chain_entry {
	future_map_fn map;
	void *map_arg;
	future_init_fn init;
	void *init_arg;
	uint64_t flags;
	struct future future;
};

#define FUTURE_CHAIN_FLAG_ENTRY_LAST		(((uint64_t)1) << 0)
#define FUTURE_CHAIN_FLAG_ENTRY_PROCESSED	(((uint64_t)1) << 1)
#define FUTURE_CHAIN_VALID_FLAGS (FUTURE_CHAIN_FLAG_ENTRY_LAST |\
	FUTURE_CHAIN_FLAG_ENTRY_PROCESSED)

enum future_chain_entry_type {
	FUTURE_CHAIN_ENTRY_REGULAR = 1,
	FUTURE_CHAIN_ENTRY_LAST = 2,
};

typedef uint8_t _future_entry_type_regular[FUTURE_CHAIN_ENTRY_REGULAR];
typedef uint8_t _future_entry_type_last[FUTURE_CHAIN_ENTRY_LAST];

#define FUTURE_CHAIN_ENTRY(_future_type, _name)\
struct {\
	future_map_fn map;\
	void *map_arg;\
	future_init_fn init;\
	void *init_arg;\
	_future_entry_type_regular *flags;\
	_future_type fut;\
} _name

#define FUTURE_CHAIN_ENTRY_LAST(_future_type, _name)\
struct {\
	future_map_fn map;\
	void *map_arg;\
	future_init_fn init;\
	void *init_arg;\
	_future_entry_type_last *flags;\
	_future_type fut;\
} _name

#define FUTURE_CHAIN_ENTRY_INIT(_entry, _fut, _map, _map_arg)\
do {\
	(_entry)->fut = (_fut);\
	(_entry)->map = (_map);\
	(_entry)->map_arg = (_map_arg);\
	(_entry)->init = NULL;\
	(_entry)->init_arg = NULL;\
	(_entry)->flags = (void *)(sizeof(*(_entry)->flags) ==\
		FUTURE_CHAIN_ENTRY_LAST ? FUTURE_CHAIN_FLAG_ENTRY_LAST : 0);\
} while (0)

#define FUTURE_CHAIN_ENTRY_LAZY_INIT(_entry, _init, _init_arg, _map, _map_arg)\
do {\
	(_entry)->map = (_map);\
	(_entry)->map_arg = (_map_arg);\
	(_entry)->init = (_init);\
	(_entry)->init_arg = (_init_arg);\
	(_entry)->flags = (void *)(sizeof(*(_entry)->flags) ==\
		FUTURE_CHAIN_ENTRY_LAST ? FUTURE_CHAIN_FLAG_ENTRY_LAST : 0);\
} while (0)

#define FUTURE_CHAIN_ENTRY_HAS_FLAG(_entry, _flag)\
(((_entry)->flags & (_flag)) == (_flag))

#define FUTURE_CHAIN_ENTRY_IS_LAST(_entry)\
FUTURE_CHAIN_ENTRY_HAS_FLAG(_entry, FUTURE_CHAIN_FLAG_ENTRY_LAST)

#define FUTURE_CHAIN_ENTRY_IS_PROCESSED(_entry)\
FUTURE_CHAIN_ENTRY_HAS_FLAG(_entry, FUTURE_CHAIN_FLAG_ENTRY_PROCESSED)

#define FUTURE_CHAIN_ENTRY_IS_INITIALIZED(_entry)\
((_entry)->init == NULL)

/*
 * TODO: Notifiers have to be copied into the state of the future, so we might
 * consider just passing it by copy here... Needs to be evaluated for
 * performance.
 */
static inline enum future_state
future_poll(struct future *fut, struct future_notifier *notifier)
{
	if (fut->context.state != FUTURE_STATE_COMPLETE) {
		fut->context.state = fut->task(&fut->context, notifier);
	}

	return fut->context.state;
}

/*
 * future_has_property -- returns 1 if a property is set and 0 otherwise.
 * It's an abstract implementation, which works for both regular and
 * chained futures.
 */
static inline int
future_has_property(struct future *fut, enum future_property property)
{
	return fut->has_property(fut, property);
}

#define FUTURE_BUSY_POLL(_futurep)\
while (future_poll(FUTURE_AS_RUNNABLE((_futurep)), NULL) !=\
	FUTURE_STATE_COMPLETE) { __FUTURE_WAIT(); }

static inline struct future_chain_entry *
get_next_future_chain_entry(struct future_context *ctx,
		struct future_chain_entry *entry, uint8_t *data,
		size_t *used_data)
{
#define _MINIASYNC_PTRSIZE sizeof(void *)
#define _MINIASYNC_ALIGN_UP(size)\
	(((size) + _MINIASYNC_PTRSIZE - 1) & ~(_MINIASYNC_PTRSIZE - 1))

	if (entry->init) {
		entry->init(&entry->future, ctx, entry->init_arg);
		entry->init = NULL;
	}
	/*
	 * `struct future` starts with a pointer, so the structure will
	 * be pointer-size aligned. We need to account for that when
	 * calculating where is the next future in a chained struct.
	 */
	*used_data += _MINIASYNC_ALIGN_UP(
			sizeof(struct future_chain_entry) +
			future_context_get_size(&entry->future.context));
	struct future_chain_entry *next = NULL;
	if (!FUTURE_CHAIN_ENTRY_IS_LAST(entry) &&
		*used_data != ctx->data_size) {
		next = (struct future_chain_entry *)(data + *used_data);
	}

#undef _MINIASYNC_PTRSIZE
#undef _MINIASYNC_ALIGN_UP

	return next;
}

static inline enum future_state
async_chain_impl(struct future_context *ctx, struct future_notifier *notifier)
{
	uint8_t *data = (uint8_t *)future_context_get_data(ctx);

	struct future_chain_entry *entry = (struct future_chain_entry *)(data);
	size_t used_data = 0;

	/*
	 * This will iterate to the first non-complete future in the chain
	 * and then call poll it once.
	 * Futures must be laid out sequentially in memory for this to work.
	 */
	while (entry != NULL) {
		struct future_chain_entry *next =
			get_next_future_chain_entry(ctx, entry,
						data, &used_data);
		if (!FUTURE_CHAIN_ENTRY_IS_PROCESSED(entry)) {
			if (future_poll(&entry->future, notifier) ==
			    FUTURE_STATE_COMPLETE) {
				if (entry->map) {
					if (next && next->init) {
						next->init(&next->future, ctx,
							next->init_arg);
						next->init = NULL;
					}
					entry->map(&entry->future.context,
							next ?
							&next->future.context
							: ctx,
							entry->map_arg);
				}
				entry->flags |=
					FUTURE_CHAIN_FLAG_ENTRY_PROCESSED;
			} else {
				return FUTURE_STATE_RUNNING;
			}
		}
		entry = next;
	}

	return FUTURE_STATE_COMPLETE;
}

static inline int
future_has_property_default(void *future, enum future_property property)
{
	/* suppres unused parameters */
	(void) (future);
	(void) (property);
	/* by default every property is set to false */
	return 0;
}

static inline int
future_chain_has_property(void *future, enum future_property property)
{
	struct future *fut = (struct future *)future;
	struct future_context *ctx = &fut->context;
	uint8_t *data = (uint8_t *)future_context_get_data(ctx);
	struct future_chain_entry *entry = (struct future_chain_entry *)(data);

	size_t used_data = 0;

	while (entry != NULL) {
		struct future_chain_entry *next =
			get_next_future_chain_entry(ctx, entry,
						data, &used_data);
		if (!FUTURE_CHAIN_ENTRY_IS_PROCESSED(entry)) {
			if ((entry->future.has_property(&entry->future,
						property)))
				return 1;
			return 0;
		}
		entry = next;
	}

	return -1;
}

#define FUTURE_CHAIN_INIT(_futurep)\
FUTURE_INIT_EXT((_futurep), async_chain_impl, future_chain_has_property)

#ifdef __cplusplus
}
#endif
#endif /* FUTURE_H */

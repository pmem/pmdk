// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019-2020, Intel Corporation */

#ifndef FUTURE_H
#define FUTURE_H 1

#include <unistd.h>
#include <stdint.h>

enum future_state {
	FUTURE_STATE_IDLE,
	FUTURE_STATE_COMPLETE,
	FUTURE_STATE_RUNNING,
};

struct future_context {
	enum future_state state;
	size_t data_size;
	size_t output_size;
};

typedef void (*future_waker_wake_fn)(void *data);

struct future_waker {
	void *data;
	future_waker_wake_fn wake;
};

void *future_context_get_data(struct future_context *context);

void *future_context_get_output(struct future_context *context);

size_t future_context_get_size(struct future_context *context);

#define FUTURE_WAKER_WAKE(_wakerp)\
((_wakerp)->wake((_wakerp)->data))

typedef enum future_state (*future_task_fn)(struct future_context *context,
				     struct future_waker waker);

struct future {
	future_task_fn task;
	struct future_context context;
};

#define FUTURE(_name, _data_type, _output_type)\
	struct _name {\
		struct future base;\
		_data_type data;\
		_output_type output;\
	}

#define FUTURE_INIT(_futurep, _taskfn)\
do {\
	(_futurep)->base.task = (_taskfn);\
	(_futurep)->base.context.state = (FUTURE_STATE_IDLE);\
	(_futurep)->base.context.data_size = sizeof((_futurep)->data);\
	(_futurep)->base.context.output_size =\
		sizeof((_futurep)->output);\
} while (0)

#define FUTURE_AS_RUNNABLE(futurep) (&(futurep)->base)
#define FUTURE_OUTPUT(futurep) (&(futurep)->output)

typedef void (*future_map_fn)(struct future_context *lhs,
			struct future_context *rhs, void *arg);

struct future_chain_entry {
	future_map_fn map;
	void *arg;
	struct future future;
};

#define FUTURE_CHAIN_ENTRY(_future_type, _name)\
struct {\
	future_map_fn map;\
	void *arg;\
	_future_type fut;\
} _name;

#define FUTURE_CHAIN_ENTRY_INIT(_entry, _fut, _map, _map_arg)\
do {\
	(_entry)->fut = (_fut);\
	(_entry)->map = _map;\
	(_entry)->arg = _map_arg;\
} while (0)

struct future_waker future_noop_waker(void);

enum future_state future_poll(struct future *fut, struct future_waker waker);

#define FUTURE_BUSY_POLL(_futurep)\
while (future_poll(FUTURE_AS_RUNNABLE((_futurep)), future_noop_waker()) !=\
	FUTURE_STATE_COMPLETE) {}

enum future_state async_chain_impl(struct future_context *ctx,
	struct future_waker waker);

#define FUTURE_CHAIN_INIT(_futurep)\
FUTURE_INIT((_futurep), async_chain_impl)

#endif

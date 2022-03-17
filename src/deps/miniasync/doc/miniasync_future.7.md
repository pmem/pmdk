---
layout: manual
Content-Style: 'text/css'
title: _MP(MINIASYNC_FUTURE, 7)
collection: miniasync
header: MINIASYNC_FUTURE
secondary_title: miniasync
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2021-2022, Intel Corporation)

[comment]: <> (miniasync_future.7 -- man page for miniasync future API)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[MACROS](#macros)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**miniasync_future** - Future API for miniasync library

# SYNOPSIS #

```c
#include <libminiasync.h>

typedef enum future_state (*future_task_fn)(struct future_context *context,
			struct future_notifier *notifier);

typedef void (*future_waker_wake_fn)(void *data);

typedef void (*future_map_fn)(struct future_context *lhs,
			struct future_context *rhs, void *arg);

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

FUTURE(_name, _data_type, _output_type)
FUTURE_INIT(_futurep, _taskfn)
FUTURE_CHAIN_ENTRY(_future_type, _name)
FUTURE_CHAIN_ENTRY_INIT(_entry, _fut, _map, _map_arg)
FUTURE_CHAIN_INIT(_futurep)
FUTURE_AS_RUNNABLE(_futurep)
FUTURE_OUTPUT(_futurep)
FUTURE_BUSY_POLL(_futurep)
FUTURE_WAKER_WAKE(_wakerp)
```

For general description of future API, see **miniasync_future**(7).

# DESCRIPTION #

Future is an abstract type representing a task, or a collection of tasks,
that can be executed incrementally by polling until the operation
is complete. Futures are typically meant to be implemented by library
developers and then used by applications to concurrently run multiple,
possibly unrelated, tasks.

A future contains the following context:
* current state of execution for the future
* a function pointer for the task
* structure for data which is the required state needed to perform the task
* structure for output to store the result of the task
* the size of the data and output structures (both can be 0)

A future definition must begin with an instance of the *struct future* type, which
contains all common metadata for all futures, followed by the structures for
data and output. The library provides convenience macros to simplify
the definition of user-defined future types. See **MACROS** section for details.

Applications must call the **future_poll**(3) method to make progress on the task
associated with the future. This function will perform an implementation-defined
operation towards completing the task and return the future's current state.
Futures are generally safe to poll until they are complete. Unless the documentation
for a specific future implementation indicates otherwise, futures can be moved in
memory and don't always have to be polled by the same thread.

Optionally, future implementations can accept notifiers for use in polling.
Notifiers can be useful to avoid busy polling when the future is waiting for some
asynchronous operation to finish or for some resource to become available.
Currently, **miniasync**(7) supports only waker notifier type.

A waker is a tuple composed of a function pointer and a data context pointer.
If a waker is supplied and consumed by a future, it will call the function with its
data pointer when the future can be polled again to make further progress. The caller
needs to make sure that the waker is safe to call until the future is complete or until
it supplies a different waker to the **future_poll**(3) method. The waker implementation
needs to be thread-safe.

A future implementation supporting **FUTURE_NOTIFIER_WAKER** type of notifier can
use a **FUTURE_WAKER_WAKE(_wakerp)** macro to signal the caller that some progress
can be made and the future should be polled again.

TODO: Mention **FUTURE_NOTIFIER_POLLER** when it becomes supported.

For more information about the usage of future API, see *examples* directory
in miniasync repository <https://github.com/pmem/miniasync>.

# MACROS #

**FUTURE(_name, _data_type, _output_type)** macro defines a future structure with *\_name*
as its name. Besides internal data needed by the future API, the defined structure contains
data member of *\_data_type* type and output member of *\_output_type* type. User can
provide the data that may later be retrieved in the future task implementation using
**future_context_get_data**(3) function. Similarly, the output of the future task can be
retrieved using **future_context_get_output**(3) function. Combined size of data and output
structures can be retrieved using **future_context_get_size**(3) function. When the user has
no need for input or output data, *\_data_type* and *\_output_type* can be defined as empty structures.

**FUTURE_INIT(_futurep, _taskfn)** macro assigns task function *\_taskfn* to the future pointed
by *\_futurep*. Task function must be of the *future_task_fn* type.

**FUTURE_CHAIN_ENTRY(_future_type, _name)** macro defines the future chain entry of the *\_future_type*
type named *\_name*. Future chain entries are defined as the members of chained future data structure
using this macro. Chained future can be composed of multiple future chain entries that will be
executed sequentially in the order they were defined.

**FUTURE_CHAIN_ENTRY_INIT(_entry, _fut, _map, _map_arg)** macro initializes the future chain
entry pointed by *\_entry*. It requires pointer to the future instance *\_fut*, address of the mapping
function *\_map* and the pointer to the argument for the mapping function *\_map_arg*. *\_fut* can either be
the instance of the future defined using **FUTURE(_name, _data_type, _output_type)** macro or a virtual
data mover future. *\_map* function must be of the *future_map_fn* type and is an optional parameter.
*map* function should define the mapping behavior of the data and output structures between chained
future entry *\_entry* that has finished and the chained future entry that is about to start its execution.
Chained future instance must initialize all of its future chain entries using this macro.

**FUTURE_CHAIN_INIT(_futurep)** macro initializes the chained future at the address *\_futurep*.

**FUTURE_AS_RUNNABLE(_futurep)** macro returns pointer to the runnable form of the future pointed by
*\_futurep*. Runnable form of the future is required as an argument in **runtime_wait**(3) and
**runtime_wait_multiple**(3) functions.

**FUTURE_OUTPUT(_futurep)** macro returns the output of the future pointed by *\_futurep*.

**FUTURE_BUSY_POLL(_futurep)** repeatedly polls the future pointed by *\_futurep* until
it completes its execution. This macro does not use optimized polling.

**FUTURE_WAKER_WAKE(_wakerp)** macro performs implementation-defined wake operation. It takes
a pointer to the waker structure of *struct future_waker* type.

# SEE ALSO #

**future_context_get_data**(3), **future_context_get_output**(3),
**future_context_get_size**(3), **future_poll**(3),
**runtime_wait**(3), **runtime_wait_multiple**(3)
**miniasync**(7), **miniasync_runtime**(7),
**miniasync_vdm**(7) and **<https://pmem.io>**

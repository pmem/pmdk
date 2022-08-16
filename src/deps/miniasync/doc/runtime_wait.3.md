---
layout: manual
Content-Style: 'text/css'
title: _MP(RUNTIME_WAIT, 3)
collection: miniasync
header: RUNTIME_WAIT
secondary_title: miniasync
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2022, Intel Corporation)

[comment]: <> (runtime_wait.3 -- man page for miniasync runtime API)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**runtime_wait**(), **runtime_wait_multiple**() - wait for the completion of single
or multiple futures

# SYNOPSIS #

```c
#include <libminiasync.h>

struct future {
	future_task_fn task;
	struct future_context context;
};

struct runtime;

void runtime_wait(struct runtime *runtime, struct future *fut);
void runtime_wait_multiple(struct runtime *runtime, struct future *futs[],
						size_t nfuts);
```

For general description of runtime API, see **miniasync_runtime**(7).

# DESCRIPTION #

The **runtime_wait**() function blocks the calling thread until the future structure
pointed by *fut* completes. While waiting the calling thread repeatedly polls the
future with **future_poll**(3) function until completion.

The **runtime_wait_multiple**() function works similar to the **runtime_wait**() function,
additionally it facilitates polling of multiple futures in an array.  **runtime_wait_multiple**()
function uniformly polls the first *nfuts* futures in the array pointed by *futs* until all
of them complete execution. Runtime execution can be influenced by future properties.
For more information about the future properties, see **miniasync_future**(7).

Properties, which affect runtime:
* **FUTURE_PROPERTY_ASYNC** property should be applied to asynchronous futures.
During **runtime_wait_multiple**() function, asynchronous futures have a priority over the
synchronous ones and, in general, are being polled first.

**miniasync**(7) runtime implementation makes use of the waker notifier feature to optimize
future polling. For more information about the waker feature, see **miniasync_future**(7).

## RETURN VALUE ##

The **runtime_wait**() function returns a pointer to a new runtime structure.

The **runtime_wait_multiple**() function does not return any value.

# SEE ALSO #

**future_poll**(3), **miniasync**(7),
**miniasync_future**(7) **miniasync_runtime**(7)
and **<https://pmem.io>**

---
layout: manual
Content-Style: 'text/css'
title: _MP(MINIASYNC_VDM_THREADS, 7)
collection: miniasync
header: MINIASYNC_VDM_THREADS
secondary_title: miniasync
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2022, Intel Corporation)

[comment]: <> (miniasync_vdm_threads.7 -- man page for miniasync vdm threads mover API)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[EXAMPLE](#example)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**miniasync_vdm_threads** - virtual data mover implementation for miniasync using
system threads

# SYNOPSIS #

```c
#include <libminiasync.h>
```

For general description of virtual data mover API, see **miniasync**(7).

# DESCRIPTION #

Thread data mover is a thread-based implementation of the virtual data mover.
Operations submitted to a thread data mover instance are queued and then executed
by one of the working threads that has taken an operation off the queue. Working threads
of each thread data mover instance are put to sleep until there's a data mover operation
to execute.

When the future is polled for the first time the data mover operation will be queued
for asynchronous execution on one of the working threads associated with the instance
of thread data mover.

Each thread data mover instance uses an internal ringbuffer for allocations associated with
data mover operations.

To create a new thread data mover instance, use **data_mover_threads_new**(3) or
**data_mover_threads_default**(3) function.

Thread data mover supports following operations:

* **vdm_memcpy**(3) - memory copy operation
* **vdm_memmove**(3) - memory move operation
* **vdm_memset**(3) - memory set operation

Thread data mover supports following notifer types:

* **FUTURE_NOTIFIER_NONE** - no notifier
* **FUTURE_NOTIFIER_WAKER** - waker

For more information about notifiers, see **miniasync_future**(7).

For more information about the usage of thread data mover API, see *examples* directory
in miniasync repository <https://github.com/pmem/miniasync>.

# EXAMPLE #

Example usage of default thread data mover **vdm_memcpy**(3) operation:
```c
struct data_mover_threads *dmt = data_mover_threads_default();
struct vdm *thread_mover = data_mover_threads_get_vdm(dmt);
struct vdm_memcpy_future memcpy_fut =
		vdm_memcpy(thread_mover, dest, src, copy_size, 0);
```

# SEE ALSO #

**data_mover_threads_default**(3), **data_mover_threads_get_vdm**(3),
**data_mover_threads_new**(3), **vdm_memcpy**(3), **vdm_memmove**(3),
**vdm_memset**(3), **miniasync**(7), **miniasync_future**(7),
**miniasync_vdm**(7) and **<https://pmem.io>**

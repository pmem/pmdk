---
layout: manual
Content-Style: 'text/css'
title: _MP(MINIASYNC, 7)
collection: miniasync
header: MINIASYNC
secondary_title: miniasync
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2021-2022, Intel Corporation)

[comment]: <> (miniasync.7 -- man page for miniasync)

[NAME](#name)<br />
[DESCRIPTION](#description)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**miniasync** - Mini Library for Asynchronous Programming in C

# DESCRIPTION #

**miniasync** is a minimalistic library used for asynchronous programming in C
programming language. It provides a **miniasync_future**(7) feature that is an
abstraction representing a task or a collection of tasks. Futures are meant to be
implemented by developers and used by applications to run multiple concurrent tasks.
Futures begin execution when they are polled for the first time, futures can be safely
polled until an **FUTURE_STATE_COMPLETE** state is returned, it means that the future
has finished its task. Polling completed future results in undefined behavior unless
the specific future implementation says otherwise. For more information about future API,
see **miniasync_future**(7).

Futures can be polled in various ways, the simplest of them all is using **future_poll**(3)
function. **miniasync** library also provides the **miniasync_runtime**(7) feature that manages
the future polling behavior. Runtime makes use of optimized polling mechanisms that avoid busy
polling and simplifies the future polling process. Runtime is meant to be used together
with concrete future implementations. For more information about runtime API, see
**miniasync_runtime**(7).

In case that the future is meant to execute an asynchronous memory operation, **miniasync** library
provides a **miniasync_vdm**(7) virtual data mover feature. Virtual data mover generalizes
asynchronous memory operations to avoid hard dependencies on any specific hardware offload
or memory operations.
**miniasync** provides the following virtual data mover implementations:

* **miniasync_vdm_threads**(7) - an implementation based on system threads

* **miniasync_vdm_dml**(7) - an implementation based on the *Data Mover Library* (**DML**)

For more information about virtual data mover API, see **miniasync_vdm**(7).

# SEE ALSO #

**future_poll**(3),
**miniasync_future**(7), **miniasync_runtime**(7),
**miniasync_vdm**(7), **miniasync_vdm_dml**(7),
**miniasync_vdm_threads**(7) and **<https://pmem.io>**

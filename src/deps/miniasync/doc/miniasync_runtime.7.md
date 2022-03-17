---
layout: manual
Content-Style: 'text/css'
title: _MP(MINIASYNC_RUNTIME, 7)
collection: miniasync
header: MINIASYNC_RUNTIME
secondary_title: miniasync
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2021-2022, Intel Corporation)

[comment]: <> (miniasync_runtime.7 -- man page for miniasync runtime API)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**miniasync_runtime** - Runtime API for miniasync library

# SYNOPSIS #

```c
#include <libminiasync.h>
```

For general description of miniasync, see **miniasync**(7).

# DESCRIPTION #

Runtime is a future polling management feature. It should be used together
with concrete implementations of the future.

**miniasync**(7) runtime provides methods for efficient polling of single or
multiple futures, **runtime_wait**(3) and **runtime_wait_multiple**(3) respectively.
It makes use of waker notifier feature to optimize future polling behavior. Thread calling
one of the wait functions polls each future until no further progress can be made, and then
goes to sleep for a period of time before repeating this process. Calling thread can be woken
ahead of schedule by the future whose implementation makes use of the waker
**FUTURE_WAKER_WAKE(_wakerp)** macro. This optimization allows the calling thread
to switch context and do some useful work instead of idle polling.
For more information about the waker feature, see **miniasync_future**(7).

There's no support for multi-threaded task scheduling.

For more information about the usage of runtime API, see *examples* directory
in miniasync repository <https://github.com/pmem/miniasync>.

# SEE ALSO #

**runtime_wait**(3), **runtime_wait_multiple**(3),
**miniasync**(7), **miniasync_future**(7),
**miniasync_vdm**(7) and **<https://pmem.io>**

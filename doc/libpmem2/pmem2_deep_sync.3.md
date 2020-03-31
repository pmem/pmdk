---
layout: manual
Content-Style: 'text/css'
title: _MP(PMEM2_DEEP_SYNC, 3)
collection: libpmem2
header: PMDK
date: pmem2 API version 1.0
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause
[comment]: <> (Copyright 2020, Intel Corporation)

[comment]: <> (pmem2_deep_sync.3 -- man page for pmem2_deep_sync)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmem2_deep_sync**() - highly reliable persistent memory synchronization

# SYNOPSIS #

```c
#include <libpmem2.h>

int pmem2_deep_sync(struct pmem2_map *map, void *ptr, size_t size)
```

# DESCRIPTION #

The **pmem2_deep_sync**() function forces any changes in the range \[*addr*, *addr*+*len*)
from the *map* to be stored durably in the most reliable persistence domain available to software,
rather than depending on automatic WPQ (write pending queue) flush on power failure (ADR).
This function requires that the provided address range was already made persistent through
regular synchronization primitives.

Since this operation is usually much more expensive than regular persist, it should
be used rarely. Typically the application should use this function only to flush
the most critical data, which are required to recover after the power failure.

# RETURN VALUE #

The **pmem2_deep_sync**() returns 0 on success or one of the following
error values on failure:

* **PMEM2_E_SYNC_RANGE** - the provided synchronization range is not a
subset of the map's address space.

* **PMEM2_E_SYNC_CACHELINE** - a low level error occurred, preventing the
data synchronization from succeeding.

# SEE ALSO #

**pmem2_get_drain_fn**(3), **pmem2_get_persist_fn**(3), **pmem2_map**(3),
**libpmem2**(7) and **<http://pmem.io>**

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

The **pmem2_deep_sync**() function forces any changes in the range \[*ptr*, *ptr*+*len*)
from the *map* to be stored durably in the most reliable persistence domain
available to software. In particular, on supported platforms, this enables
the code not to rely on automatic cache or WPQ (write pending queue) flush on power failure (ADR/eADR).
This function requires that the provided address range was already made persistent
through regular synchronization primitives.

Since this operation is usually much more expensive than regular persist, it
should be used sparingly. Typically, the application should only ever use this
function as a precaution against hardware failures, e.g., in code that detects
silent data corruption caused by unsafe shutdown (see more in **libpmem2_unsafe_shutdown**(7)).

# RETURN VALUE #

The **pmem2_deep_sync**() returns 0 on success or one of the following
error values on failure:

* **PMEM2_E_SYNC_RANGE** - the provided synchronization range is not a
subset of the map's address space.

* **PMEM2_E_NOSUPP** - the platform has no facilities for deep synchronization of
Device Dax.

* -**errno** set by failing **write**(2), while trying to use the Device Dax
*deep_flush* interface.

* -**errno** set by failing **msync**(2), while trying to perform a deep
synchronization on a regular DAX volume.

* -**errno** set by failing **realpath**(3), while trying to determine whether
fd points to a Device DAX.

# SEE ALSO #

**pmem2_get_drain_fn**(3), **pmem2_get_persist_fn**(3), **pmem2_map**(3),
**libpmem2**(7) and **<http://pmem.io>**

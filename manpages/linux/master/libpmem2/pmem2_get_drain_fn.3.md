---
layout: manual
Content-Style: 'text/css'
title: PMEM2_GET_DRAIN_FN
collection: libpmem2
header: PMDK
date: pmem2 API version 1.0
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause
[comment]: <> (Copyright 2020, Intel Corporation)

[comment]: <> (pmem2_get_drain_fn.3 -- man page for pmem2_get_drain_fn)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmem2_get_drain_fn**() - get a drain function

# SYNOPSIS #

```c
#include <libpmem2.h>

typedef void (*pmem2_drain_fn)(void);

struct pmem2_map;

pmem2_drain_fn pmem2_get_drain_fn(struct pmem2_map *map);
```

# DESCRIPTION #

The **pmem2_get_drain_fn**() function returns a pointer to a function
responsible for efficiently draining flushes (see **pmem2_get_flush_fn**(3))
in the range owned by *map*. Draining, in this context, means making sure
that the flushes before this operation won't be reordered after it.
While it is not strictly true, draining can be thought of as waiting for
previous flushes to complete.

If two (or more) mappings share the same drain function, it is safe to call
this function once for all flushes belonging to those mappings.

# RETURN VALUE #

The **pmem2_get_drain_fn**() function never returns NULL.

**pmem2_get_drain_fn**() for the same *map* always returns the same function.
This means that it's safe to cache its return value. However, this function
is very cheap (because it returns a precomputed value), so caching may not
be necessary.

# SEE ALSO #

**pmem2_get_flush_fn**(3), **pmem2_get_persist_fn**(3), **pmem2_map_new**(3),
**libpmem2**(7) and **<http://pmem.io>**

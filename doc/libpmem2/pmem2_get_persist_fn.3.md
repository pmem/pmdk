---
draft: false
slider_enable: true
description: ""
disclaimer: "The contents of this web site and the associated <a href=\"https://github.com/pmem\">GitHub repositories</a> are BSD-licensed open source."
aliases: ["pmem2_get_persist_fn.3.html"]
title: "libpmem2 | PMDK"
header: "pmem2 API version 1.0"
---

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2020, Intel Corporation)

[comment]: <> (pmem2_get_persist_fn.3 -- man page for pmem2_get_persist_fn)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmem2_get_persist_fn**() - get a persist function

# SYNOPSIS #

```c
#include <libpmem2.h>

typedef void (*pmem2_persist_fn)(const void *ptr, size_t size);

struct pmem2_map;

pmem2_persist_fn pmem2_get_persist_fn(struct pmem2_map *map);
```

# DESCRIPTION #

The **pmem2_get_persist_fn**() function returns a pointer to a function
responsible for efficiently persisting data in the range owned by the *map*.

Persisting data using *pmem2_persist_fn* guarantees that the data is stored
durably by the time it returns.

There are no alignment restrictions on the range described by *ptr* and *size*,
but *pmem2_persist_fn* may expand the range as necessary to meet platform
alignment requirements.

There is nothing atomic or transactional about *pmem2_persist_fn*. Any
unwritten stores in the given range will be written, but some stores may have
already been written by virtue of normal cache eviction/replacement policies.
Correctly written code must not depend on stores waiting until
*pmem2_persist_fn* is called to become persistent -- they can become persistent
at any time before *pmem2_persist_fn* is called.

If two (or more) mappings share the same *pmem2_persist_fn* and they are
adjacent to each other, it is safe to call this function for a range spanning
those mappings.

Internally *pmem2_persist_fn* performs two operations:

- memory flush (**pmem2_get_flush_fn**(3)), which can be reordered by
the CPU with other flushes
- drain (**pmem2_get_drain_fn**(3)), which makes sure that the flushes
before this operation won't be reordered after it

So this code:

```c
pmem2_persist_fn persist_fn = pmem2_get_persist_fn(map);
persist_fn(addr, len);
```

is equivalent of:

```c
pmem2_flush_fn flush_fn = pmem2_get_flush_fn(map);
pmem2_drain_fn drain_fn = pmem2_get_drain_fn(map);

flush_fn(addr, len);
drain_fn();
```

Advanced applications may want to flush multiple discontiguous regions
and perform the drain operation only once.

# RETURN VALUE #

The **pmem2_get_persist_fn**() function never returns NULL.

The **pmem2_get_persist_fn**() for the same *map* always returns the same function.
This means that it's safe to cache its return value. However, this function
is very cheap (because it returns a precomputed value), so caching may not be
necessary.

# SEE ALSO #

**pmem2_get_drain_fn**(3), **pmem2_get_flush_fn**(3), **pmem2_map_new**(3),
**libpmem2**(7) and **<https://pmem.io>**

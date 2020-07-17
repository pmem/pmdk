---
layout: manual
Content-Style: 'text/css'
title: PMEM2_GET_FLUSH_FN
collection: libpmem2
header: PMDK
date: pmem2 API version 1.0
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause
[comment]: <> (Copyright 2020, Intel Corporation)

[comment]: <> (pmem2_get_flush_fn.3 -- man page for pmem2_get_flush_fn)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmem2_get_flush_fn**() - get a flush function

# SYNOPSIS #

```c
#include <libpmem2.h>

typedef void (*pmem2_flush_fn)(const void *ptr, size_t size);

struct pmem2_map;

pmem2_flush_fn pmem2_get_flush_fn(struct pmem2_map *map);
```

# DESCRIPTION #

The **pmem2_get_flush_fn**() function returns a pointer to a function
responsible for efficiently flushing data in the range owned by the *map*.

Flushing data using *pmem2_flush_fn* **does not** guarantee that the data
is stored durably by the time it returns. To get this guarantee, application
should either use the persist operation (see **pmem2_get_persist_fn**(3))
or follow *pmem2_flush_fn* by a drain operation (see **pmem2_get_drain_fn**(3)).

There are no alignment restrictions on the range described by *ptr* and *size*,
but *pmem2_flush_fn* may expand the range as necessary to meet platform
alignment requirements.

There is nothing atomic or transactional about *pmem2_flush_fn*. Any
unwritten stores in the given range will be written, but some stores may have
already been written by virtue of normal cache eviction/replacement policies.
Correctly written code must not depend on stores waiting until
*pmem2_flush_fn* is called to be flushed -- they can be flushed
at any time before *pmem2_flush_fn* is called.

If two (or more) mappings share the same *pmem2_flush_fn* and they are
adjacent to each other, it is safe to call this function for a range spanning
those mappings.

# RETURN VALUE #

The **pmem2_get_flush_fn**() function never returns NULL.

**pmem2_get_flush_fn**() for the same *map* always returns the same function.
This means that it's safe to cache its return value. However, this function
is very cheap (because it returns a precomputed value), so caching may not
be necessary.

# SEE ALSO #

**pmem2_get_drain_fn**(3), **pmem2_get_persist_fn**(3), **pmem2_map_new**(3),
**libpmem2**(7) and **<http://pmem.io>**

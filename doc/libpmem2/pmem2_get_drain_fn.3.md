---
layout: manual
Content-Style: 'text/css'
title: _MP(PMEM2_GET_DRAIN_FN, 3)
collection: libpmem2
header: PMDK
date: pmem2 API version 1.0
...

[comment]: <> (Copyright 2020, Intel Corporation)

[comment]: <> (Redistribution and use in source and binary forms, with or without)
[comment]: <> (modification, are permitted provided that the following conditions)
[comment]: <> (are met:)
[comment]: <> (    * Redistributions of source code must retain the above copyright)
[comment]: <> (      notice, this list of conditions and the following disclaimer.)
[comment]: <> (    * Redistributions in binary form must reproduce the above copyright)
[comment]: <> (      notice, this list of conditions and the following disclaimer in)
[comment]: <> (      the documentation and/or other materials provided with the)
[comment]: <> (      distribution.)
[comment]: <> (    * Neither the name of the copyright holder nor the names of its)
[comment]: <> (      contributors may be used to endorse or promote products derived)
[comment]: <> (      from this software without specific prior written permission.)

[comment]: <> (THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS)
[comment]: <> ("AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT)
[comment]: <> (LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR)
[comment]: <> (A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT)
[comment]: <> (OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,)
[comment]: <> (SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT)
[comment]: <> (LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,)
[comment]: <> (DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY)
[comment]: <> (THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT)
[comment]: <> ((INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE)
[comment]: <> (OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.)

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

**pmem2_get_flush_fn**(3), **pmem2_get_persist_fn**(3), **pmem2_map**(3),
**libpmem2**(7) and **<http://pmem.io>**

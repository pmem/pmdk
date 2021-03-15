---
layout: manual
Content-Style: 'text/css'
title: _MP(PMEM2_GET_MEMMOVE_FN, 3)
collection: libpmem2
header: PMDK
date: pmem2 API version 1.0
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause
[comment]: <> (Copyright 2020, Intel Corporation)

[comment]: <> (pmem2_get_memmove_fn.3 -- man page for pmem2_get_memmove_fn)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmem2_get_memmove_fn**(), **pmem2_get_memset_fn**(),
**pmem2_get_memcpy_fn**() - get a function that provides
        optimized copying to persistent memory

# SYNOPSIS #

```c
#include <libpmem2.h>

typedef void *(*pmem2_memmove_fn)(void *pmemdest, const void *src, size_t len,
		unsigned flags);
typedef void *(*pmem2_memcpy_fn)(void *pmemdest, const void *src, size_t len,
		unsigned flags);
typedef void *(*pmem2_memset_fn)(void *pmemdest, int c, size_t len,
		unsigned flags);

struct pmem2_map;

pmem2_memmove_fn pmem2_get_memmove_fn(struct pmem2_map *map);
pmem2_memset_fn pmem2_get_memset_fn(struct pmem2_map *map);
pmem2_memcpy_fn pmem2_get_memcpy_fn(struct pmem2_map *map);
```

# DESCRIPTION #

The **pmem2_get_memmove_fn**(), **pmem2_get_memset_fn**(),
**pmem2_get_memcpy_fn**() functions return a pointer to a function
responsible for efficient storing and flushing of data for mapping *map*.

**pmem2_memmove_fn**(), **pmem2_memset_fn**() and **pmem2_memcpy_fn**()
functions provide the same memory copying functionalities as their namesakes
**memmove**(3), **memcpy**(3) and **memset**(3), and ensure that the result has
been flushed to persistence before returning (unless **PMEM2_F_MEM_NOFLUSH** flag was used).

For example, the following code:

```c
        memmove(dest, src, len);
        pmem2_persist_fn persist_fn = pmem2_get_persist_fn(map);
        persist_fn(dest, len);
```
is functionally equivalent to:

```c
        pmem2_memmove_fn memmove_fn = pmem2_get_memmove_fn(map);
        memmove_fn(dest, src, len, 0);
```

Unlike libc implementation, **libpmem2** functions guarantee that if destination
buffer address and length are 8 byte aligned then all stores will be performed
using at least 8 byte store instructions. This means that a series of 8 byte
stores followed by *persist_fn* can be safely replaced by a single *memmove_fn* call.

The *flags* argument of all of the above functions has the same meaning.
It can be 0 or a bitwise OR of one or more of the following flags:

+ **PMEM2_F_MEM_NODRAIN** - modifies the behavior to skip the final
  *pmem2_drain_fn* step. This allows applications to optimize cases where
  several ranges are being copied to persistent memory, followed by a single
  call to *pmem2_drain_fn*. The following example illustrates how this flag
  might be used to avoid multiple calls to *pmem2_drain_fn* when copying several
  ranges of memory to pmem:

```c
pmem2_memcpy_fn memcpy_fn = pmem2_get_memcpy_fn(map);
pmem2_drain_fn drain_fn = pmem2_get_drain_fn(map);

/* ... write several ranges to pmem ... */
memcpy_fn(pmemdest1, src1, len1, PMEM2_F_MEM_NODRAIN);
memcpy_fn(pmemdest2, src2, len2, PMEM2_F_MEM_NODRAIN);

/* ... */

/* wait for any pmem stores to drain from HW buffers */
drain_fn();
```

+ **PMEM2_F_MEM_NOFLUSH** - Don't flush anything. This implies **PMEM2_F_MEM_NODRAIN**.
  Using this flag only makes sense when it's followed by any function that
  flushes data.

The remaining flags say *how* the operation should be done, and are merely hints.

+ **PMEM2_F_MEM_NONTEMPORAL** - Use non-temporal instructions.
  This flag is mutually exclusive with **PMEM2_F_MEM_TEMPORAL**.
  On x86\_64 this flag is mutually exclusive with **PMEM2_F_MEM_NOFLUSH**.

+ **PMEM2_F_MEM_TEMPORAL** - Use temporal instructions.
  This flag is mutually exclusive with **PMEM2_F_MEM_NONTEMPORAL**.

+ **PMEM2_F_MEM_WC** - Use write combining mode.
  This flag is mutually exclusive with **PMEM2_F_MEM_WB**.
  On x86\_64 this flag is mutually exclusive with **PMEM2_F_MEM_NOFLUSH**.

+ **PMEM2_F_MEM_WB** - Use write back mode.
  This flag is mutually exclusive with **PMEM2_F_MEM_WC**.
  On x86\_64 this is an alias for **PMEM2_F_MEM_TEMPORAL**.

Using an invalid combination of flags has undefined behavior.

Without any of the above flags **libpmem2** will try to guess the best strategy
based on the data size. See **PMEM_MOVNT_THRESHOLD** description in **libpmem2**(7) for
details.

# RETURN VALUE #

The **pmem2_get_memmove_fn**(), **pmem2_get_memset_fn**(),
**pmem2_get_memcpy_fn**() functions never return NULL.

They return the same function for the same mapping.

This means that it's safe to cache their return values. However, these functions
are very cheap (because their return values are precomputed), so caching may not
be necessary.

If two (or more) mappings share the same *pmem2_memmove_fn*, *pmem2_memset_fn*,
*pmem2_memcpy_fn* and they are adjacent to each other, it is safe to call these
functions for a range spanning those mappings.

# SEE ALSO #

**memcpy**(3), **memmove**(3), **memset**(3), **pmem2_get_drain_fn**(3),
**pmem2_get_memcpy_fn**(3), **pmem2_get_memset_fn**(3), **pmem2_map_new**(3),
**pmem2_get_persist_fn**(3), **libpmem2**(7) and **<https://pmem.io>**

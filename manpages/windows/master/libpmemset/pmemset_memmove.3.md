---
layout: manual
Content-Style: 'text/css'
title: PMEMSET_MEMMOVE
collection: libpmemset
header: PMDK
date: pmemset API version 1.0
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2021, Intel Corporation)

[comment]: <> (pmemset_memmove.3 -- man page for libpmemset pmemset_memmove function)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmemset_memmove**(), **pmemset_memcpy**(),
**pmemset_pmemset**() - performs memmove/memcpy/memset
on memory from the *pmemset*.

# SYNOPSIS #

```c
#include <libpmemset.h>

void *pmemset_memmove(struct pmemset *set, void *pmemdest, const void *src,
		size_t len, unsigned flags);

void *pmemset_memcpy(struct pmemset *set, void *pmemdest, const void *src,
		size_t len, unsigned flags);

void *pmemset_memset(struct pmemset *set, void *pmemdest, int c, size_t len,
		unsigned flags);
```

# DESCRIPTION #

The **pmemset_memmove**(), **pmemset_memcpy**() and **pmemset_memset**()
functions provide the same memory copying functionalities as their namesakes
**memmove**(3), **memcpy**(3) and **memset**(3), and ensure that the result has
been flushed to persistence before returning (unless **PMEM2_F_MEM_NOFLUSH** flag was used).

For example, the following code:

```c
        memmove(dest, src, len);
        pmemset_persist(set, dest, len);
```
is functionally equivalent to:

```c
        pmemset_memmove(set, dest, src, len, 0);
```

Unlike libc implementation, **libpmemset** functions guarantee that if destination
buffer address and length are 8 byte aligned then all stores will be performed
using at least 8 byte store instructions. This means that a series of 8 byte
stores followed by *pmemset_persist* can be safely replaced by a single *pmemset_memmove*
call.

The *flags* argument of all of the above functions has the same meaning.
It can be 0 or a bitwise OR of one or more of the following flags:

+ **PMEMSET_F_MEM_NODRAIN** - modifies the behavior to skip the final
  *pmemset_drain* step. This allows applications to optimize cases where
  several ranges are being copied to persistent memory, followed by a single
  call to **pmemset_drain**(3). The following example illustrates how this flag
  might be used to avoid multiple calls to **pmemset_drain**(3) when copying several
  ranges of memory to pmem:

```c
/* ... write several ranges to pmem ... */
pmemset_memcpy(set, pmemdest1, src1, len1, PMEMSET_F_MEM_NODRAIN);
pmemset_memcpy(set, pmemdest2, src2, len2, PMEMSET_F_MEM_NODRAIN);

/* ... */

/* wait for any pmem stores to drain from HW buffers */
pmemset_drain(set);
```

+ **PMEMSET_F_MEM_NOFLUSH** - Don't flush anything. This implies **PMEMSET_F_MEM_NODRAIN**.
  Using this flag only makes sense when it's followed by any function that
  flushes data.

The remaining flags say *how* the operation should be done, and are merely hints.

+ **PMEMSET_F_MEM_NONTEMPORAL** - Use non-temporal instructions.
  This flag is mutually exclusive with **PMEMSET_F_MEM_TEMPORAL**.
  On x86\_64 this flag is mutually exclusive with **PMEMSET_F_MEM_NOFLUSH**.

+ **PMEMSET_F_MEM_TEMPORAL** - Use temporal instructions.
  This flag is mutually exclusive with **PMEMSET_F_MEM_NONTEMPORAL**.

+ **PMEMSET_F_MEM_WC** - Use write combining mode.
  This flag is mutually exclusive with **PMEMSET_F_MEM_WB**.
  On x86\_64 this flag is mutually exclusive with **PMEMSET_F_MEM_NOFLUSH**.

+ **PMEMSET_F_MEM_WB** - Use write back mode.
  This flag is mutually exclusive with **PMEMSET_F_MEM_WC**.
  On x86\_64 this is an alias for **PMEMSET_F_MEM_TEMPORAL**.

Using an invalid combination of flags has undefined behavior.

Without any of the above flags **libpmemset** will try to guess the best strategy
based on the data size. See **PMEM_MOVNT_THRESHOLD** description in **libpmem2**(7) for
details.

# RETURN VALUE #

The **pmemset_memmove**(), **pmemset_memset**(), **pmemset_memcpy**() function
returns a pointer to the memory area *pmemdest* in the same way as their namesakes
**memmove**(3), **memcpy**(3) and **memset**(3).

# SEE ALSO #

**memcpy**(3), **memmove**(3), **memset**(3),
**pmemset_drain**(3), **pmemset_persist**(3),
**libpmem2**(7), **libpmemset**(7) and **<https://pmem.io>**

---
draft: false
slider_enable: true
description: ""
disclaimer: "The contents of this web site and the associated <a href=\"https://github.com/pmem\">GitHub repositories</a> are BSD-licensed open source."
aliases: ["pmemobj_memcpy_persist.3.html"]
title: "libpmemobj | PMDK"
header: "pmemobj API version 2.3"
---

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2017-2018, Intel Corporation)

[comment]: <> (pmemobj_memcpy_persist.3 -- man page for Low-level memory manipulation)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[EXAMPLES](#examples)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmemobj_persist**(), **pmemobj_xpersist**(), **pmemobj_flush**(),
**pmemobj_xflush**(), **pmemobj_drain**(), **pmemobj_memcpy**(),
**pmemobj_memmove**(), **pmemobj_memset**(), **pmemobj_memcpy_persist**(),
**pmemobj_memset_persist**() - low-level memory manipulation functions

# SYNOPSIS #

```c
#include <libpmemobj.h>

void pmemobj_persist(PMEMobjpool *pop, const void *addr,
	size_t len);
void pmemobj_flush(PMEMobjpool *pop, const void *addr,
	size_t len);
void pmemobj_drain(PMEMobjpool *pop);

int pmemobj_xpersist(PMEMobjpool *pop, const void *addr,
	size_t len, unsigned flags);
int pmemobj_xflush(PMEMobjpool *pop, const void *addr,
	size_t len, unsigned flags);

void *pmemobj_memcpy(PMEMobjpool *pop, void *dest,
	const void *src, size_t len, unsigned flags);
void *pmemobj_memmove(PMEMobjpool *pop, void *dest,
	const void *src, size_t len, unsigned flags);
void *pmemobj_memset(PMEMobjpool *pop, void *dest,
	int c, size_t len, unsigned flags);

void *pmemobj_memcpy_persist(PMEMobjpool *pop, void *dest,
	const void *src, size_t len);
void *pmemobj_memset_persist(PMEMobjpool *pop, void *dest,
	int c, size_t len);
```

# DESCRIPTION #

The **libpmemobj**-specific low-level memory manipulation functions described
here leverage the knowledge of the additional configuration options available
for **libpmemobj**(7) pools, such as replication. They also take advantage of
the type of storage behind the pool and use appropriate flush/drain functions.
It is advised to use these functions in conjunction with **libpmemobj**(7)
objects rather than using low-level memory manipulation functions from
**libpmem**.

**pmemobj_persist**() forces any changes in the range \[*addr*, *addr*+*len*)
to be stored durably in persistent memory. Internally this may call either
**pmem_msync**(3) or **pmem_persist**(3). There are no alignment restrictions
on the range described by *addr* and *len*, but **pmemobj_persist**() may
expand the range as necessary to meet platform alignment requirements.

>WARNING:
Like **msync**(2), there is nothing atomic or transactional about this call.
Any unwritten stores in the given range will be written, but some stores may
have already been written by virtue of normal cache eviction/replacement
policies. Correctly written code must not depend on stores waiting until
**pmemobj_persist**() is called to become persistent - they can become
persistent at any time before **pmemobj_persist**() is called.

The **pmemobj_flush**() and **pmemobj_drain**() functions provide partial
versions of the **pmemobj_persist**() function described above.
These functions allow advanced programs to create their own variations of
**pmemobj_persist**().
For example, a program that needs to flush several discontiguous ranges can
call **pmemobj_flush**() for each range and then follow up by calling
**pmemobj_drain**() once. For more information on partial flushing operations,
see **pmem_flush**(3).

**pmemobj_xpersist**() is a version of **pmemobj_persist**() function with
additional *flags* argument.
It supports only the **PMEMOBJ_F_RELAXED** flag.
This flag indicates that memory transfer operation does
not require 8-byte atomicity guarantees.

**pmemobj_xflush**() is a version of **pmemobj_flush**() function with
additional *flags* argument.
It supports only the **PMEMOBJ_F_RELAXED** flag.

The **pmemobj_memmove**(), **pmemobj_memcpy**() and **pmemobj_memset**() functions
provide the same memory copying as their namesakes **memmove**(3), **memcpy**(3),
and **memset**(3), and ensure that the result has been flushed to persistence
before returning (unless **PMEMOBJ_MEM_NOFLUSH** flag was used).
Valid flags for those functions:

+ **PMEMOBJ_F_RELAXED** - This flag indicates that memory transfer operation
  does not require 8-byte atomicity guarantees.

+ **PMEMOBJ_F_MEM_NOFLUSH** - Don't flush anything.
  This implies **PMEMOBJ_F_MEM_NODRAIN**.
  Using this flag only makes sense when it's followed by any function that
  flushes data.

The remaining flags say *how* the operation should be done, and are merely hints.

+ **PMEMOBJ_F_MEM_NONTEMPORAL** - Use non-temporal instructions.
  This flag is mutually exclusive with **PMEMOBJ_F_MEM_TEMPORAL**.
  On x86\_64 this flag is mutually exclusive with **PMEMOBJ_F_MEM_NOFLUSH**.

+ **PMEMOBJ_F_MEM_TEMPORAL** - Use temporal instructions.
  This flag is mutually exclusive with **PMEMOBJ_F_MEM_NONTEMPORAL**.

+ **PMEMOBJ_F_MEM_WC** - Use write combining mode.
  This flag is mutually exclusive with **PMEMOBJ_F_MEM_WB**.
  On x86\_64 this is an alias for **PMEMOBJ_F_MEM_NONTEMPORAL**.
  On x86\_64 this flag is mutually exclusive with **PMEMOBJ_F_MEM_NOFLUSH**.

+ **PMEMOBJ_F_MEM_WB** - Use write back mode.
  This flag is mutually exclusive with **PMEMOBJ_F_MEM_WC**.
  On x86\_64 this is an alias for **PMEMOBJ_F_MEM_TEMPORAL**.

**pmemobj_memcpy_persist**() is an alias for **pmemobj_memcpy**() with flags equal to 0.

**pmemobj_memset_persist**() is an alias for **pmemobj_memset**() with flags equal to 0.

# RETURN VALUE #

**pmemobj_memmove**(), **pmemobj_memcpy**(), **pmemobj_memset**(),
**pmemobj_memcpy_persist**() and **pmemobj_memset_persist**() return destination
buffer.

**pmemobj_persist**(), **pmemobj_flush**() and **pmemobj_drain**()
do not return any value.

**pmemobj_xpersist**() and **pmemobj_xflush**() returns non-zero value and
sets errno to EINVAL only if not supported flags has been provided.

# EXAMPLES #

The following code is functionally equivalent to
**pmemobj_memcpy_persist**():

```c
void *
pmemobj_memcpy_persist(PMEMobjpool *pop, void *dest,
	const void *src, size_t len)
{
	void *retval = memcpy(dest, src, len);

	pmemobj_persist(pop, dest, len);

	return retval;
}
```

**pmemobj_persist**() can be thought of as this:

```c
void
pmemobj_persist(PMEMobjpool *pop, const void *addr, size_t len)
{
	/* flush the processor caches */
	pmemobj_flush(pop, addr, len);

	/* wait for any pmem stores to drain from HW buffers */
	pmemobj_drain(pop);
}
```

# SEE ALSO #

**memcpy**(3), **memset**(3), **pmem_msync**(3),
**pmem_persist**(3), **libpmem**(7) **libpmemobj**(7)
and **<https://pmem.io>**

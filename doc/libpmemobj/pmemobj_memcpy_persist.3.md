---
layout: manual
Content-Style: 'text/css'
title: _MP(PMEMOBJ_MEMCPY_PERSIST, 3)
collection: libpmemobj
header: PMDK
date: pmemobj API version 2.3
...

[comment]: <> (Copyright 2017-2018, Intel Corporation)

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

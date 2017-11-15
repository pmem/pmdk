---
layout: manual
Content-Style: 'text/css'
title: _MP(PMEM_FLUSH, 3)
collection: libpmem
header: NVM Library
date: pmem API version 1.0
...

[comment]: <> (Copyright 2017, Intel Corporation)

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

[comment]: <> (pmem_flush.3 -- man page for partial flushing operations

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />


# NAME #

**pmem_flush**(), **pmem_drain**(), **pmem_persist**(), **pmem_msync**(),
**pmem_has_hw_drain**() -- check persistency, store persistent data and delete mappings


# SYNOPSIS #

```c
#include <libpmem.h>

void pmem_persist(const void *addr, size_t len);
int pmem_msync(const void *addr, size_t len);
void pmem_flush(const void *addr, size_t len);
void pmem_drain(void);
int pmem_has_hw_drain(void);
```


# DESCRIPTION #

The functions in this section provide access to the stages of flushing
to persistence, for the less common cases where an application needs more
control of the flushing operations than the **pmem_persist**() function.


>WARNING:
Using **pmem_persist**() on a range where **pmem_is_pmem**(3)
returns false may not do anything useful -- use **msync**(2) instead.

The **pmem_persist**() function force any changes in the range
\[*addr*, *addr*+*len*) to be stored durably in
persistent memory. This is equivalent to calling **msync**(2)
but may be more optimal and will avoid calling into the kernel if
possible. There are no alignment restrictions on the range described by
*addr* and *len*, but **pmem_persist**() may expand the range as
necessary to meet platform alignment requirements.

>WARNING:
Like **msync**(2), there is nothing atomic or transactional
about this call. Any unwritten stores in the given range will be
written, but some stores may have already been written by virtue of
normal cache eviction/replacement policies. Correctly written code must
not depend on stores waiting until **pmem_persist**() is called to
become persistent -- they can become persistent at any time before
**pmem_persist**() is called.

The **pmem_msync**() function is like **pmem_persist**() in that it
forces any changes in the range \[*addr*, *addr*+*len*) to be stored
durably. Since it calls **msync**(), this function works on either
persistent memory or a memory mapped file on traditional storage.
**pmem_msync**() takes steps to ensure the alignment of addresses and
lengths passed to **msync**() meet the requirements of that system call.
It calls **msync**() with the **MS_SYNC** flag as described in
**msync**(2). Typically the application only checks for the existence of
persistent memory once, and then uses that result throughout the
program, for example:

```c
/* do this call once, after the pmem is memory mapped */
int is_pmem = pmem_is_pmem(rangeaddr, rangelen);

/* ... make changes to a range of pmem ... */

/* make the changes durable */
if (is_pmem)
	pmem_persist(subrangeaddr, subrangelen);
else
	pmem_msync(subrangeaddr, subrangelen);

/* ... */
```

_WINUX(,=q=
>WARNING:
On Linux, **pmem_msync**() and **msync**(2) have no effect on memory ranges
mapped from Device DAX.  In case of memory ranges where **pmem_is_pmem**(3)
returns true use **pmem_persist**() to force the changes to be stored durably
in persistent memory.
=e=)

The **pmem_flush**() and **pmem_drain**() functions provide
partial versions of the **pmem_persist**() function.
**pmem_persist**() can be thought of as this:

```c
void
pmem_persist(const void *addr, size_t len)
{
	/* flush the processor caches */
	pmem_flush(addr, len);

	/* wait for any pmem stores to drain from HW buffers */
	pmem_drain();
}
```

These functions allow advanced programs to create their own variations
of **pmem_persist**(). For example, a program that needs to flush
several discontiguous ranges can call **pmem_flush**() for each range
and then follow up by calling **pmem_drain**() once.

The **pmem_has_hw_drain**() function checks if the machine
supports an explicit *hardware drain*
instruction for persistent memory.


# RETURN VALUE #

The **pmem_persist**() function returns no value.

The **pmem_msync**() return value is the return value of
**msync**(), which can return -1 and set *errno* to indicate an error.

The **pmem_flush**() and **pmem_drain**() functions return no value.

The **pmem_has_hw_drain**() function returns true if the machine
supports an explicit *hardware drain*
instruction for persistent memory.
On Intel processors with persistent memory,
stores to persistent memory are considered persistent
once they are flushed from the CPU caches, so this
function always returns false. Despite that, programs using
**pmem_flush**() to flush ranges of memory should still follow up by calling
**pmem_drain**() once to ensure the flushes are complete. As mentioned above,
**pmem_persist**() handles calling both **pmem_flush**() and **pmem_drain**().


# SEE ALSO #

**msync**(2), **pmem_is_pmem**(3), **libpmem**(7)
and **<http://pmem.io>**

---
layout: manual
Content-Style: 'text/css'
title: PMEM_FLUSH!3
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

**pmem_flush**(), **pmem_drain**(),
**pmem_has_hw_drain**() -- check persistency, store persistent
									data and delete mappings


# SYNOPSIS #

```c
#include <libpmem.h>

void pmem_flush(const void *addr, size_t len);
void pmem_drain(void);
int pmem_has_hw_drain(void);
```


# DESCRIPTION #

The functions in this section provide access to the stages of flushing
to persistence, for the less common cases where an application needs more
control of the flushing operations than the **pmem_persist**(3) function.

The **pmem_flush**() and **pmem_drain**() functions provide
partial versions of the **pmem_persist**(3) function.
**pmem_persist**(3) can be thought of as this:

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
of **pmem_persist**(3). For example, a program that needs to flush
several discontiguous ranges can call **pmem_flush**() for each range
and then follow up by calling **pmem_drain**() once.

The **pmem_has_hw_drain**() function checks if the machine
supports an explicit *hardware drain*
instruction for persistent memory.


# RETURN VALUE #

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
**pmem_persist**(3) handles calling both **pmem_flush**() and **pmem_drain**().


# SEE ALSO #

**pmem_persist**(3), **libpmem**(7) and **<http://pmem.io>**

---
layout: manual
Content-Style: 'text/css'
title: PMEM_MEMMOVE_PERSIST
collection: libpmem
header: PMDK
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

[comment]: <> (pmem_memmove_persist.3 -- man page for functions that provide optimized copying to persistent memory

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />


# NAME #

**pmem_memmove_persist**(), **pmem_memcpy_persist**(), **pmem_memset_persist**(),
**pmem_memmove_nodrain**(), **pmem_memcpy_nodrain**(), **pmem_memset_nodrain**()
-- functions that provide optimized copying to persistent memory


# SYNOPSIS #

```c
#include <libpmem.h>

void *pmem_memmove_persist(void *pmemdest, const void *src, size_t len);
void *pmem_memcpy_persist(void *pmemdest, const void *src, size_t len);
void *pmem_memset_persist(void *pmemdest, int c, size_t len);
void *pmem_memmove_nodrain(void *pmemdest, const void *src, size_t len);
void *pmem_memcpy_nodrain(void *pmemdest, const void *src, size_t len);
void *pmem_memset_nodrain(void *pmemdest, int c, size_t len);
```


# DESCRIPTION #

The **pmem_memmove_persist**(), **pmem_memcpy_persist**(), and
**pmem_memset_persist**(), functions provide the same memory copying
as their namesakes **memmove**(3), **memcpy**(3) and **memset**(3), and
ensure that the result has been flushed to persistence before returning.
For example, the following code is functionally equivalent to
**pmem_memmove_persist**():

```c
void *
pmem_memmove_persist(void *pmemdest, const void *src, size_t len)
{
	void *retval = memmove(pmemdest, src, len);

	pmem_persist(pmemdest, len);

	return retval;
}
```

Calling **pmem_memmove_persist**() may out-perform the above code,
however, since the **libpmem**(7) implementation may take advantage of the
fact that *pmemdest* is persistent memory and use instructions such as
*non-temporal* stores to avoid the need to flush processor caches.

>WARNING:
Using these functions where **pmem_is_pmem**(3) returns false
may not do anything useful. Use the normal libc functions in that case.


The **pmem_memmove_nodrain**(), **pmem_memcpy_nodrain**() and
**pmem_memset_nodrain**() functions are similar to
**pmem_memmove_persist**(), **pmem_memcpy_persist**(), and
**pmem_memset_persist**() described above, except they skip the final
**pmem_drain**() step. This allows applications to optimize cases where
several ranges are being copied to persistent memory, followed by a
single call to **pmem_drain**(). The following example illustrates how
these functions might be used to avoid multiple calls to
**pmem_drain**() when copying several ranges of memory to pmem:

```c
/* ... write several ranges to pmem ... */
pmem_memcpy_nodrain(pmemdest1, src1, len1);
pmem_memcpy_nodrain(pmemdest2, src2, len2);

/* ... */

/* wait for any pmem stores to drain from HW buffers */
pmem_drain();
```


# RETURN VALUE #

The **pmem_memmove_persist**(), **pmem_memcpy_persist**(), **pmem_memset_persist**(),
**pmem_memmove_nodrain**(), **pmem_memcpy_nodrain**() and **pmem_memset_nodrain**()
functions return the address of the destination.


# CAVEATS #
After calling any of the *\_nodrain* functions (**pmem_memmove_nodrain**(),
**pmem_memcpy_nodrain**() or **pmem_memset_nodrain**()) you should not expect
memory to be visible to other threads before calling **pmem_drain**(3) or any
of the *\_persist* functions.  This is because those functions may use non-temporal
store instructions, which are weakly ordered. See "Intel 64 and IA-32 Architectures
Software Developer's Manual", Volume 1, "Caching of Temporal vs. Non-Temporal
Data" section for details.


# SEE ALSO #

**memcpy**(3), **memmove**(3), **memset**(3),
**libpmem**(7) and **<http://pmem.io>**

---
layout: manual
Content-Style: 'text/css'
title: _MP(PMEMCTO_ALIGNED_ALLOC, 3)
collection: libpmemcto
header: PMDK
date: libpmemcto API version 1.0
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

[comment]: <> (pmemcto_aligned_alloc.3 -- man page for libpmemcto)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[ERRORS](#errors)<br />
[SEE ALSO](#see-also)<br />


# NAME #

pmemcto_aligned_alloc -- allocate aligned memory


# SYNOPSIS #

```c
#include <libpmemcto.h>

void *pmemcto_aligned_alloc(PMEMctopool *pcp, size_t alignment, size_t size);
```


# DESCRIPTION #

The **pmemcto_aligned_alloc**() function provides the same semantics
as **aligned_alloc**(3), but operates on the memory pool *pcp* instead
of the process heap supplied by the system.  It allocates *size* bytes from
the memory pool and returns a pointer to the allocated memory.
The memory is not zeroed.
The memory address will be a multiple of *alignment*, which must be a power
of two.


# RETURN VALUE #

On success, **pmemcto_aligned_alloc**() function returns a pointer to the
allocated memory.
If *size* is 0, then **pmemcto_aligned_alloc**() returns either NULL,
or a unique pointer value that can later be successfully passed
to **pmemcto_free**(3).  If **pmemcto_aligned_alloc**() is unable to satisfy
the allocation request, a NULL pointer is returned and *errno* is set
appropriately.


# ERRORS #

**EINVAL** *alignment* was not a power of two.

**ENOMEM** Insufficient memory available to satisfy allocation request.


# SEE ALSO #

**pmemcto_malloc**(3), **aligned_alloc**(3), **malloc**(3),
**jemalloc**(3), **libpmemcto**(7) and **<http://pmem.io>**

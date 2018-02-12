---
layout: manual
Content-Style: 'text/css'
title: _MP(PMEMCTO_MALLOC_USABLE_SIZE, 3)
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

[comment]: <> (pmemcto_malloc_usable_size.3 -- man page for libpmemcto)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />


# NAME #

pmemcto_malloc_usable_size -- obtain size of block of memory allocated from pool


# SYNOPSIS #

```c
#include <libpmemcto.h>

size_t pmemcto_malloc_usable_size(PMEMctopool *pcp, void *ptr);
```


# DESCRIPTION #


The **pmemcto_malloc_usable_size**() function provides the same semantics
as **malloc_usable_size**(3), but operates on the memory pool *pcp* instead
of the process heap supplied by the system.  It returns the number of usable
bytes in the block of allocated memory pointed to by *ptr*, a pointer to
a block of memory allocated by **pmemcto_malloc**(3) or a related function.


# RETURN VALUE #

The **pmemcto_malloc_usable_size**() function returns the number of usable
bytes in the block of allocated memory pointed to by *ptr*.
If *ptr* is NULL, 0 is returned.


# SEE ALSO #

**jemalloc**(3), **malloc**(3), **malloc_usable_size**(3),
**pmemcto_malloc**(3),
**libpmemcto**(7) and **<http://pmem.io>**

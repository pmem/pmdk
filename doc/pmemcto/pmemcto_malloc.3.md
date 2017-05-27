---
layout: manual
Content-Style: 'text/css'
title: PMEMCTO_MALLOC(3)
header: NVM Library
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

[comment]: <> (pmemcto_malloc.3 -- man page for libpmemcto)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[ERRORS](#errors)<br />
[NOTES](#notes)<br />
[ACKNOWLEDGEMENTS](#acknowledgements)<br />
[SEE ALSO](#see-also)<br />


# NAME #

pmemcto_malloc, pmemcto_free, pmemcto_calloc, pmemcto_realloc
-- allocate and free persistent memory


# SYNOPSIS #

```c
#include <libpmemcto.h>

void *pmemcto_malloc(PMEMctopool *pcp, size_t size);
void pmemcto_free(PMEMctopool *pcp, void *ptr);
void *pmemcto_calloc(PMEMctopool *pcp, size_t nmemb, size_t size);
void *pmemcto_realloc(PMEMctopool *pcp, void *ptr, size_t size);
```

# DESCRIPTION #

The **pmemcto_malloc**() function provides the same semantics
as **malloc**(3), but operates on the memory pool *pcp* instead of
the process heap supplied by the system.  It allocates *size* bytes and
returns a pointer to the allocated memory.  *The memory is not initialized*.
If *size* is 0, then **pmemcto_malloc**() returns either NULL, or a unique
pointer value that can later be successfully passed to **pmemcto_free**().

The **pmemcto_free**() function provides the same semantics as **free**(3),
but operates on the memory pool *pcp* instead of the process heap supplied
by the system.  It frees the memory space pointed to by *ptr*, which must
have been returned by a previous call to **pmemcto_malloc**(),
**pmemcto_calloc**() or **pmemcto_realloc**() for *the same pool of memory*.
Undefined behavior occurs if frees do not correspond to allocated memory
from the same memory pool.  If *ptr* is NULL, no operation is performed.

The **pmemcto_calloc**() function provides the same semantics
as **calloc**(3), but operates on the memory pool *pcp* instead of
the process heap supplied by the system.  It allocates memory for an array
of *nmemb* elements of *size* bytes each and returns a pointer to the
allocated memory.  The memory is set to zero.  If *nmemb* or *size* is 0,
then **pmemcto_calloc**() returns either NULL, or a unique pointer value
that can later be successfully passed to **pmemcto_free**().

The **pmemcto_realloc**() function provides the same semantics
as **realloc**(3), but operates on the memory pool *pcp* instead of
the process heap supplied by the system.  It changes the size of
the memory block pointed to by *ptr* to *size* bytes.  The contents will be
unchanged in the range from the start of the region up to the minimum
of the old and new sizes.  If the new size is larger than the old size,
the added memory will *not* be initialized.  If *ptr* is NULL,
then the call is equivalent to *pmemcto_malloc(pcp, size)*, for all values
of *size*; if *size* is equal to zero and *ptr* is not NULL, then the call
is equivalent to *pmemcto_free(pcp, ptr)*.  Unless *ptr* is NULL,
it must have been returned by an earlier call to **pmemcto_malloc**(),
**pmemcto_calloc**() or **pmemcto_realloc**().
If the area pointed to was moved, a *pmemcto_free(pcp, ptr)* is done.


# RETURN VALUE #

The **pmemcto_malloc**() and **pmemcto_calloc**() functions return
a pointer to the allocated memory.  If the allocation request cannot be
satisfied, a NULL pointer is returned and *errno* is set appropriately.

The **pmemcto_free**() function returns no value.

The **pmemcto_realloc**() function returns a pointer to the newly
allocated memory, or NULL if it is unable to satisfy the allocation request.
If *size* was equal to 0, either NULL or a pointer suitable to be passed
to **pmemcto_free**() is returned.  If **pmemcto_realloc**() fails
the original block is left untouched; it is not freed or moved.


# ERRORS #

**ENOMEM** Insufficient memory available to satisfy allocation request.


# NOTES #

Unlike the normal **malloc**(), which asks the system for additional
memory when it runs out, **libpmemcto** allocates the size it is told
to and never attempts to grow or shrink that memory pool.


# ACKNOWLEDGEMENTS #

**libpmemcto** depends on jemalloc, written by Jason Evans, to do the heavy
lifting of managing dynamic memory allocation. See:
<http://www.canonware.com/jemalloc>

**libpmemcto** builds on the persistent memory programming model recommended
by the SNIA NVM Programming Technical Work Group:
<http://snia.org/nvmp>


# SEE ALSO #

**libpmemcto**(3), **malloc**(3), **jemalloc**(3)
and **<http://pmem.io>**

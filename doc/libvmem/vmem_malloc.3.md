---
layout: manual
Content-Style: 'text/css'
title: _MP(VMEM_MALLOC, 3)
collection: libvmem
header: PMDK
date: vmem API version 1.1
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

[comment]: <> (vmem_malloc.3 -- man page for memory allocation related functions)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />


# NAME #

**vmem_malloc**(), **vmem_calloc**(), **vmem_realloc**(),
**vmem_free**(), **vmem_aligned_alloc**(), **vmem_strdup**(),
**vmem_wcsdup**(), **vmem_malloc_usable_size**() - memory allocation related functions


# SYNOPSIS #

```c
#include <libvmem.h>

void *vmem_malloc(VMEM *vmp, size_t size);
void vmem_free(VMEM *vmp, void *ptr);
void *vmem_calloc(VMEM *vmp, size_t nmemb, size_t size);
void *vmem_realloc(VMEM *vmp, void *ptr, size_t size);
void *vmem_aligned_alloc(VMEM *vmp, size_t alignment, size_t size);
char *vmem_strdup(VMEM *vmp, const char *s);
wchar_t *vmem_wcsdup(VMEM *vmp, const wchar_t *s);
size_t vmem_malloc_usable_size(VMEM *vmp, void *ptr);
```


# DESCRIPTION #

This section describes the *malloc*-like API provided by **libvmem**(7).
These functions provide the same semantics as their libc namesakes,
but operate on the memory pools specified by their first arguments.

The **vmem_malloc**() function provides the same semantics as **malloc**(3),
but operates on the memory pool *vmp* instead of the process heap
supplied by the system. It allocates specified *size* bytes.

The **vmem_free**() function provides the same semantics as **free**(3),
but operates on the memory pool *vmp* instead of the process heap
supplied by the system. It frees the memory space pointed to by *ptr*, which
must have been returned by a previous call to **vmem_malloc**(),
**vmem_calloc**() or **vmem_realloc**() for *the same pool of memory*.
If *ptr* is NULL, no operation is performed.

The **vmem_calloc**() function provides the same semantics as **calloc**(3),
but operates on the memory pool *vmp* instead of the process heap
supplied by the system. It allocates memory for an array of *nmemb* elements of
*size* bytes each. The memory is set to zero.

The **vmem_realloc**() function provides the same semantics as **realloc**(3),
but operates on the memory pool *vmp* instead of the process heap supplied by
the system. It changes the size of the memory block pointed to by *ptr* to
*size* bytes. The contents will be unchanged in the range from the start of the
region up to the minimum of the old and new sizes. If the new size is larger
than the old size, the added memory will *not* be initialized.

Unless *ptr* is NULL, it must have been returned by an earlier call to
**vmem_malloc**(), **vmem_calloc**() or **vmem_realloc**(). If *ptr* is NULL,
then the call is equivalent to *vmem_malloc(vmp, size)*, for all values of
*size*; if *size* is equal to zero, and *ptr* is not NULL, then the call
is equivalent to *vmem_free(vmp, ptr)*.

The **vmem_aligned_alloc**() function provides the same semantics as
**aligned_alloc**(3), but operates on the memory pool *vmp* instead of
the process heap supplied by the system. It allocates *size* bytes from
the memory pool. The memory address will be a multiple of *alignment*,
which must be a power of two.

The **vmem_strdup**() function provides the same semantics as **strdup**(3),
but operates on the memory pool *vmp* instead of the process heap supplied by the
system. Memory for the new string is obtained with **vmem_malloc**(), on the given
memory pool, and can be freed with **vmem_free**() on the same memory pool.

The **vmem_wcsdup**() function provides the same semantics as **wcsdup**(3),
but operates on the memory pool *vmp* instead of the process heap supplied by the
system. Memory for the new string is obtained with **vmem_malloc**(), on the given
memory pool, and can be freed with **vmem_free**() on the same memory pool.

The **vmem_malloc_usable_size**() function provides the same semantics as
**malloc_usable_size**(3), but operates on the memory pool *vmp* instead of the
process heap supplied by the system.


# RETURN VALUE #

On success, **vmem_malloc**() returns a pointer to the allocated memory.
If *size* is 0, then **vmem_malloc**() returns either NULL,
or a unique pointer value that can later be successfully passed to
**vmem_free**(). If **vmem_malloc**() is unable to satisfy the allocation
request, it returns NULL and sets *errno* appropriately.

The **vmem_free**() function returns no value.
Undefined behavior occurs if frees do not correspond to allocated memory
from the same memory pool.

On success, **vmem_calloc**() returns a pointer to the allocated memory. If
*nmemb* or *size* is 0, then **vmem_calloc**() returns either NULL, or a unique
pointer value that can later be successfully passed to **vmem_free**().
If **vmem_calloc**() is unable to satisfy the allocation request, it returns
NULL and sets *errno* appropriately.

On success, **vmem_realloc**() returns a pointer to the allocated memory, which
may be different from *ptr*. If the area pointed to was moved, a
*vmem_free(vmp, ptr)* is done. If **vmem_realloc**() is unable to satisfy the
allocation request, it returns NULL and sets *errno* appropriately.

On success, **vmem_aligned_alloc**() returns a pointer to the allocated memory.
If **vmem_aligned_alloc**() is unable to satisfy the allocation request, it
returns NULL and sets *errno* appropriately.

On success, **vmem_strdup**() returns a pointer to a new string which is a
duplicate of the string *s*. If **vmem_strdup**() is unable to satisfy the
allocation request, it returns NULL and sets *errno* appropriately.

On success, **vmem_wcsdup**() returns a pointer to a new wide character string
which is a duplicate of the wide character string *s*. If **vmem_wcsdup**()
is unable to satisfy the allocation request, it returns NULL and sets *errno*
appropriately.

The **vmem_malloc_usable_size**() function returns the number of usable bytes
in the block of allocated memory pointed to by *ptr*, a pointer to a block of
memory allocated by **vmem_malloc**() or a related function. If *ptr* is NULL,
0 is returned.


# SEE ALSO #

**calloc**(3), **free**(3), **malloc**(3), **malloc_usable_size**(3),
**realloc**(3), **strdup**(3), **wcsdup**(3) **libvmem(7)** and **<http://pmem.io>**

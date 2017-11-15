---
layout: manual
Content-Style: 'text/css'
title: VMEM_CREATE
collection: libvmem
header: NVM Library
date: vmem API version 1.1
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

[comment]: <> (vmem_create.3 -- man page for volatile memory pool management functions)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />


# NAME #

**vmem_createU**()/**vmem_createW**(), **vmem_create_in_region**(), **vmem_delete**(),
**vmem_check**(), **vmem_stats_print**() -- volatile memory pool management


# SYNOPSIS #

```c
#include <libvmem.h>

VMEM *vmem_createU(const char *dir, size_t size);
VMEM *vmem_createW(const wchar_t *dir, size_t size);
VMEM *vmem_create_in_region(void *addr, size_t size);
void vmem_delete(VMEM *vmp);
int vmem_check(VMEM *vmp);
void vmem_stats_print(VMEM *vmp, const char *opts);
```

>NOTE: NVML API supports UNICODE. If **NVML_UTF8_API** macro is defined then
basic API functions are expanded to UTF-8 API with postfix *U*,
otherwise they are expanded to UNICODE API with postfix *W*.


# DESCRIPTION #

To use **libvmem**, a *memory pool* is first created. This is most commonly
done with the **vmem_createU**()/**vmem_createW**() function described in this section. The other
**libvmem** functions are for less common cases, where applications have
special needs for creating pools or examining library state.

The **vmem_createU**()/**vmem_createW**() function creates a memory pool. The resulting pool is then
used with functions like **vmem_malloc**() and **vmem_free**() to provide the
familiar *malloc-like* programming model for the memory pool.

The **vmem_createU**()/**vmem_createW**() function creates the pool by allocating a temporary file in the
given directory *dir*. The file is created in a fashion similar to
**tmpfile**(3), so that the file name does not appear when
the directory is listed and the space is automatically freed when the program
terminates. *size* bytes are allocated and the resulting space is memory-mapped.
The minimum *size* value allowed by the library is
defined in **\<libvmem.h\>** as **VMEM_MIN_POOL**. The maximum allowed size is not
limited by **libvmem**, but by the file system specified by the *dir* argument.
The *size* passed in is the raw size of the memory pool and **libvmem** will
use some of that space for its own metadata.

This function can also be called with the **dir** argument pointing to a device
DAX, and in that case the entire device will serve as a volatile pool.
Device DAX is the device-centric analogue of Filesystem DAX. It allows memory
ranges to be allocated and mapped without need of an intervening file system.
For more information please see **ndctl-create-namespace**(1).

The **vmem_create_in_region**() is an alternate **libvmem** entry point
for creating a memory pool. It is for the rare case where an application
needs to create a memory pool from an already memory-mapped region.
Instead of allocating space from a given file system, **vmem_create_in_region**()
is given the memory region explicitly via the *addr* and *size* arguments.
Any data in the region is lost by calling **vmem_create_in_region**(),
which will immediately store its own data structures for managing the pool there.
Like **vmem_createU**()/**vmem_createW**() above, the minimum *size* allowed is defined as **VMEM_MIN_POOL**.
The *addr* argument must be page aligned.

The **vmem_delete**() function releases the memory pool *vmp*.
If the memory pool was created using **vmem_createU**()/**vmem_createW**(), deleting it
allows the space to be reclaimed.

The **vmem_check**() function performs an extensive consistency
check of all **libvmem** internal data structures in memory pool *vmp*.
Since an error return indicates memory pool corruption, applications
should not continue to use a pool in this state. Additional details about
errors found are logged when the log level is at least 1 (see **DEBUGGING AND
ERROR HANDLING** section in **libvmem**(7)). During the consistency check
performed by **vmem_check**(), other operations on the same memory pool are
locked out. The checks are all read-only; **vmem_check**() never modifies the
memory pool. This function is mostly useful for **libvmem** developers during
testing/debugging.

The **vmem_stats_print**() function produces messages containing statistics
about the given memory pool. The output is printed using **libvmem**'s internal
*print_func* function (see **vmem_set_funcs**() from **MANAGING LIBRARY
BEHAVIOR** section in **libvmem**(7)). That means the output typically
appears on *stderr* unless the caller supplies a replacement *print_func* or
sets the environment variable **VMEM_LOG_FILE** to direct output elsewhere.
The *opts* string can either be NULL or it can contain a list of
options that change the stats printed. General information that never changes
during execution can be omitted by specifying "g" as a character within the
opts string. The characters "m" and "a" can be specified to omit merged arena
and per arena statistics, respectively; "b" and "l" can be specified to omit
per size class statistics for bins and large objects, respectively. Unrecognized
characters are silently ignored. Note that thread caching may prevent some statistics
from being completely up to date. See **jemalloc**(3) for more detail
(the description of the available *opts* above was taken from that man page).


# RETURN VALUE #

The **vmem_createU**()/**vmem_createW**() function returns an opaque memory pool handle, of type *VMEM\**
or NULL if an error occurred (in which case *errno* is set appropriately).
Returned opaque memory pool handle is then used with the other functions
from **libvmem** that operate on a specific memory pool.
Calling **vmem_createU**()/**vmem_createW**() with a size smaller than **VMEM_MIN_POOL**  will return an error.

The **vmem_create_in_region**() function returns an opaque
memory pool handle or NULL if an error occurred (in which case *errno* is set
appropriately). Undefined behavior occurs if *addr* does not point to the contiguous
memory region in the virtual address space of the calling process, or if the *size*
is larger than the actual size of the memory region pointed by *addr*.

The **vmem_delete**() function returns no value.

The **vmem_check**() function returns 1 if the memory pool during the
check is found to be consistent and 0 otherwise. Cases where the check
couldn't be performed, are indicated by a return value of -1.

The **vmem_stats_print**() function returns no value.


# SEE ALSO #

**ndctl-create-namespace**(1), **jemalloc**(3), **tmpfile**(3),
**libvmem**(7) and **<http://pmem.io>**

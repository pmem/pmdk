---
layout: manual
Content-Style: 'text/css'
title: PMEM_IS_PMEM!3
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

[comment]: <> (pmem_is_pmem.3 -- man page for most commonly used functions from libpmem library)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[NOTES](#notes)<br />
[SEE ALSO](#see-also)<br />


# NAME #

**pmem_is_pmem**(), **pmem_persist**(), **pmem_msync**(),
!pmem_map_file, **pmem_unmap**() -- check persistency, store persistent
									data and delete mappings


# SYNOPSIS #

```c
#include <libpmem.h>

int pmem_is_pmem(const void *addr, size_t len);
void pmem_persist(const void *addr, size_t len);
int pmem_msync(const void *addr, size_t len);
!ifdef{WIN32}
{
void *pmem_map_fileU(const char *path, size_t len, int flags,
	mode_t mode, size_t *mapped_lenp, int *is_pmemp);
void *pmem_map_fileW(const wchar_t *path, size_t len, int flags,
	mode_t mode, size_t *mapped_lenp, int *is_pmemp);
}{
void *pmem_map_file(const char *path, size_t len, int flags,
	mode_t mode, size_t *mapped_lenp, int *is_pmemp);
}
int pmem_unmap(void *addr, size_t len);
```

!ifdef{WIN32}
{
>NOTE: NVML API supports UNICODE. If **NVML_UTF8_API** macro is defined then
basic API functions are expanded to UTF-8 API with postfix *U*,
otherwise they are expanded to UNICODE API with postfix *W*.
}

# DESCRIPTION #

Most pmem-aware applications will take advantage of higher level
libraries that alleviate the application from calling into **libpmem**
directly. Application developers that wish to access raw memory mapped
persistence directly (via **mmap**(2)) and that wish to take on the
responsibility for flushing stores to persistence will find the
functions described in this section to be the most commonly used.

The **pmem_is_pmem**() function detects if the entire range
\[*addr*, *addr*+*len*) consists of persistent memory.
The implementation of **pmem_is_pmem**() requires a non-trivial amount
of work to determine if the given range is entirely persistent memory.
For this reason, it is better to call **pmem_is_pmem**() once when a
range of memory is first encountered, save the result, and use the saved
result to determine whether **pmem_persist**() or **msync**(2) is
appropriate for flushing changes to persistence. Calling
**pmem_is_pmem**() each time changes are flushed to persistence will
not perform well.

>WARNING:
Using **pmem_persist**() on a range where **pmem_is_pmem**()
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

>WARNING:
On Linux, **pmem_msync**() and **msync**(2) have no effect on memory ranges
mapped from Device DAX.  In case of memory ranges where **pmem_is_pmem**()
returns true use **pmem_persist**() to force the changes to be stored durably
in persistent memory.

The !pmem_map_file function creates a new read/write
mapping for the given *path* file. It will map the file using **mmap**(2),
but it also takes extra steps to make large page mappings more likely.

On success, !pmem_map_file returns a pointer to mapped area. If
*mapped_lenp* is not NULL, the length of the mapping is also stored at
the address it points to. The *is_pmemp* argument, if non-NULL, points
to a flag that **pmem_is_pmem**() sets to say if the mapped file is
actual pmem, or if **msync**() must be used to flush writes for the
mapped range. On error, NULL is returned, *errno* is set appropriately,
and *mapped_lenp* and *is_pmemp* are left untouched.

The *flags* argument can be 0 or bitwise OR of one or more of the
following file creation flags:

+ **PMEM_FILE_CREATE** - Create the named file if it does not exist.
  *len* must be non-zero and specifies the size of the file to be created.
  *mode* has the same meaning as for **open**(2) and specifies the mode to
  use in case a new file is created. If neither **PMEM_FILE_CREATE** nor
  **PMEM_FILE_TMPFILE** is specified, then *mode* is ignored.

+ **PMEM_FILE_EXCL** - Same meaning as **O_EXCL** on **open**(2) -
  Ensure that this call creates the file. If this flag is specified in
  conjunction with **PMEM_FILE_CREATE**, and pathname already exists,
  then !pmem_map_file will fail.

+ **PMEM_FILE_TMPFILE** - Same meaning as **O_TMPFILE** on **open**(2).
  Create a mapping for an unnamed temporary file. **PMEM_FILE_CREATE**
  and *len* must be specified and *path* must be an existing directory
  name.

+ **PMEM_FILE_SPARSE** - When creating a file, create a sparse (holey)
  file instead of calling **posix_fallocate**(2). Valid only if specified
  in conjunction with **PMEM_FILE_CREATE** or **PMEM_FILE_TMPFILE**,
  otherwise ignored.

If creation flags are not supplied, then !pmem_map_file creates a
mapping for an existing file. In such case, *len* should be zero. The
entire file is mapped to memory; its length is used as the length of the
mapping and returned via *mapped_lenp*.

The path of a file can point to a Device DAX and in such case only
**PMEM_FILE_CREATE** and **PMEM_FILE_SPARSE** flags are valid, but they both
effectively do nothing. For Device DAX mappings, the *len* argument must be,
regardless of the flags, equal to either 0 or the exact size of the device.

To delete mappings created with !pmem_map_file, use **pmem_unmap**().

The **pmem_unmap**() function deletes all the mappings for the
specified address range, and causes further references to addresses
within the range to generate invalid memory references. It will use the
address specified by the parameter *addr*, where *addr* must be a
previously mapped region. **pmem_unmap**() will delete the mappings
using the **munmap**(2).


# RETURN VALUE #

The **pmem_is_pmem**() function returns true only if the entire range
\[*addr*, *addr*+*len*) consists of persistent memory. A true return
from **pmem_is_pmem**() means it is safe to use **pmem_persist**()
and the related functions to make changes durable for that memory
range.

The **pmem_persist**() function returns no value.

The **pmem_msync**() return value is the return value of
**msync**(), which can return -1 and set *errno* to indicate an error.

The !pmem_map_file function returns no value.

The **pmem_unmap**() function on success returns zero.
On error, -1 is returned, and *errno* is set appropriately.


# NOTES #

On Linux, **pmem_is_pmem**() returns true only if the entire range
is mapped directly from Device DAX (/dev/daxX.Y) without an intervening
file system.  In the future, as file systems become available that support
flushing with **pmem_persist**(), **pmem_is_pmem**() will return true
as appropriate.


# SEE ALSO #

**mmap**(2),  **msync**(2), **munmap**(2), **posix_fallocate**(2),
**libpmem**(7) and **<http://pmem.io>**

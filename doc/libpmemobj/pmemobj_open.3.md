---
layout: manual
Content-Style: 'text/css'
title: _MP(PMEMOBJ_OPEN, 3)
collection: libpmemobj
header: NVM Library
date: pmemobj API version 2.2
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

[comment]: <> (pmemobj_open.3 -- man page for most commonly used functions from libpmemobj library)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />


# NAME #

!pmemobj_open, !pmemobj_create,
**pmemobj_close**(), !pmemobj_check
-- create, open and close persistent memory transactional object store


# SYNOPSIS #

```c
#include <libpmemobj.h>

!ifdef{WIN32}
{
PMEMobjpool *pmemobj_openU(const char *path, const char *layout);
PMEMobjpool *pmemobj_openW(const wchar_t *path, const wchar_t *layout);
PMEMobjpool *pmemobj_createU(const char *path, const char *layout,
	size_t poolsize, mode_t mode);
PMEMobjpool *pmemobj_createW(const wchar_t *path, const wchar_t *layout,
	size_t poolsize, mode_t mode);
}{
PMEMobjpool *pmemobj_open(const char *path, const char *layout);
PMEMobjpool *pmemobj_create(const char *path, const char *layout,
	size_t poolsize, mode_t mode);
}
void pmemobj_close(PMEMobjpool *pop);
!ifdef{WIN32}
{
int pmemobj_checkU(const char *path, const char *layout);
int pmemobj_checkW(const wchar_t *path, const wchar_t *layout);
}{
int pmemobj_check(const char *path, const char *layout);
}
```

_UNICODE()


# DESCRIPTION #

To use the pmem-resident transactional object store provided by
**libpmemobj**(7), a *memory pool* is first created. This is done
with the !pmemobj_create function described in this section.
The other functions described in this section then operate
on the resulting memory pool.

Additionally, none of the three functions described below are thread-safe with
respect to any other **libpmemobj** functions. In other words, when creating,
opening or deleting a pool, nothing else in the library can happen in parallel.

Once created, the memory pool is represented by an opaque handle,
of type *PMEMobjpool\**, which is passed to most of the other functions
in this section. Internally, **libpmemobj** will use either **pmem_persist**(3)
or **msync**(2) when it needs to flush changes, depending on whether the memory
pool appears to be persistent memory or a regular file (see the
**pmem_is_pmem**(3) function in **libpmem**(7) for more information). There is
no need for applications to flush changes directly when using the object
memory API provided by **libpmemobj**.

The _UW(pmemobj_create) function creates a transactional object store with the
given total *poolsize*. *path* specifies the name of the memory pool file to be
created. *layout* specifies the application's layout type in the form of a
string. The layout name is not interpreted by **libpmemobj**, but may be used
as a check when _UW(pmemobj_open) is called. The layout name, including the
terminating null byte ('\0'), cannot be longer than **PMEMOBJ_MAX_LAYOUT** as
defined in **\<libpmemobj.h\>**. A NULL *layout* is equivalent
to using an empty string as a layout name. *mode* specifies the permissions to
use when creating the file, as described by **creat**(2). The memory pool file
is fully allocated to the size *poolsize* using **posix_fallocate**(3). The
caller may choose to take responsibility for creating the memory pool file
by creating it before calling _UW(pmemobj_create), and then specifying
*poolsize* as zero. In this case _UW(pmemobj_create) will take the pool size
from the size of the existing file and will verify that the file appears to be
empty by searching for any non-zero data in the pool header at the beginning of
the file. The minimum net pool size allowed by the library for a local
transactional object store is defined in **\<libpmemobj.h\>** as
**PMEMOBJ_MIN_POOL**. _WINUX(,=q=For remote replicas the minimum file size
is defined in **\<librpmem.h\>** as **RPMEM_MIN_PART**.=e=)

The _UW(pmemobj_open) function opens an existing object store memory pool.
*path* must be an existing file containing a pmemobj memory pool as created
by _UW(pmemobj_create). If *layout* is non-NULL, it is compared to the layout
name provided to _UW(pmemobj_create) when the pool was first created. This can
be used to verify that the layout of the pool matches what was expected.
The application must have permission to open the file and memory map it with
read/write permissions.

The **pmemobj_close**() function closes the memory pool indicated by *pop* and
deletes the memory pool handle. The object store itself lives on in the file
that contains it and may be re-opened at a later time using
!pmemobj_open as described above.

The !pmemobj_check function performs a consistency check of the file indicated by
*path*. !pmemobj_check opens the given *path* read-only so
it never makes any changes to the file. This function is not supported on Device DAX.

# RETURN VALUE #

The !pmemobj_create function returns memory pool handle used with
most of the functions in **libpmemobj**(7). On error it returns NULL
and sets *errno* appropriately.

The !pmemobj_open function returns a memory pool handle used with
most of the functions in in **libpmemobj**(7). If an error prevents the pool
from being opened, or if the given *layout* does not match the pool's layout,
!pmemobj_open returns NULL and sets *errno* appropriately.

The **pmemobj_close**() function returns no value.

The !pmemobj_check function returns 1 if the memory pool is found to be consistent. Any
inconsistencies found will cause !pmemobj_check to return 0, in which case the use of
the file with **libpmemobj** will result in undefined behavior. The debug version of
**libpmemobj** will provide additional details on inconsistencies when **PMEMOBJ_LOG_LEVEL**
is at least 1, as described in the **DEBUGGING ANDERROR HANDLING** section in **libpmemobj**(7).
!pmemobj_check will return -1 and set *errno* if it cannot perform the consistency
check due to other errors.


# SEE ALSO #

**creat**(2), **msync**(2), **pmem_is_pmem**(3), **pmem_persist**(3),
**posix_fallocate**(3), **libpmem**(7), **libpmemobj**(7)
and **<http://pmem.io>**

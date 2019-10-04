---
layout: manual
Content-Style: 'text/css'
title: PMEMOBJ_OPEN
collection: libpmemobj
header: PMDK
date: pmemobj API version 2.3
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

[comment]: <> (pmemobj_open.3 -- man page for most commonly used functions from libpmemobj library)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[CAVEATS](#caveats)<br />
[SEE ALSO](#see-also)<br />


# NAME #

**pmemobj_openU**()/**pmemobj_openW**(), **pmemobj_createU**()/**pmemobj_createW**(),
**pmemobj_close**(), **pmemobj_checkU**()/**pmemobj_checkW**()
- create, open, close and validate persistent memory transactional object store


# SYNOPSIS #

```c
#include <libpmemobj.h>

PMEMobjpool *pmemobj_openU(const char *path, const char *layout);
PMEMobjpool *pmemobj_openW(const wchar_t *path, const char *layout);
PMEMobjpool *pmemobj_createU(const char *path, const char *layout,
	size_t poolsize, mode_t mode);
PMEMobjpool *pmemobj_createW(const wchar_t *path, const char *layout,
	size_t poolsize, mode_t mode);
void pmemobj_close(PMEMobjpool *pop);
int pmemobj_checkU(const char *path, const char *layout);
int pmemobj_checkW(const wchar_t *path, const char *layout);
```


>NOTE: The PMDK API supports UNICODE. If the **PMDK_UTF8_API** macro is
defined, basic API functions are expanded to the UTF-8 API with postfix *U*.
Otherwise they are expanded to the UNICODE API with postfix *W*.


# DESCRIPTION #

To use the pmem-resident transactional object store provided by
**libpmemobj**(7), a *memory pool* must first be created
with the **pmemobj_createU**()/**pmemobj_createW**() function described below. Existing pools
may be opened with the **pmemobj_openU**()/**pmemobj_openW**() function.

None of the three functions described below are thread-safe with respect
to any other **libpmemobj**(7) function. In other words, when creating,
opening or deleting a pool, nothing else in the library can happen in parallel,
and therefore these functions should be called from the main thread.

Once created, the memory pool is represented by an opaque handle,
of type *PMEMobjpool\**, which is passed to most of the other **libpmemobj**(7)
functions. Internally, **libpmemobj**(7) will use either **pmem_persist**(3)
or **msync**(2) when it needs to flush changes, depending on whether the memory
pool appears to be persistent memory or a regular file (see the
**pmem_is_pmem**(3) function in **libpmem**(7) for more information). There is
no need for applications to flush changes directly when using the object
memory API provided by **libpmemobj**(7).

The **pmemobj_createU**()/**pmemobj_createW**() function creates a transactional object store with the
given total *poolsize*. *path* specifies the name of the memory pool file to be
created. *layout* specifies the application's layout type in the form of a
string. The layout name is not interpreted by **libpmemobj**(7), but may be
used as a check when **pmemobj_openU**()/**pmemobj_openW**() is called. The layout name, including
the terminating null byte ('\0'), cannot be longer than **PMEMOBJ_MAX_LAYOUT**
as defined in **\<libpmemobj.h\>**. A NULL *layout* is equivalent
to using an empty string as a layout name. *mode* specifies the permissions to
use when creating the file, as described by **creat**(2). The memory pool file
is fully allocated to the size *poolsize* using **posix_fallocate**(3). The
caller may choose to take responsibility for creating the memory pool file
by creating it before calling **pmemobj_createU**()/**pmemobj_createW**(), and then specifying
*poolsize* as zero. In this case **pmemobj_createU**()/**pmemobj_createW**() will take the pool size
from the size of the existing file and will verify that the file appears to be
empty by searching for any non-zero data in the pool header at the beginning of
the file. The minimum net pool size allowed by the library for a local
transactional object store is defined in **\<libpmemobj.h\>** as
**PMEMOBJ_MIN_POOL**. 

Depending on the configuration of the system, the available non-volatile
memory space may be divided into multiple memory devices.
In such case, the maximum size of the pmemobj memory pool
could be limited by the capacity of a single memory device.
**libpmemobj**(7) allows building persistent memory
resident object store spanning multiple memory devices by creation of
persistent memory pools consisting of multiple files, where each part of
such a *pool set* may be stored on a different memory device
or pmem-aware filesystem.

Creation of all the parts of the pool set can be done with **pmemobj_createU**()/**pmemobj_createW**();
however, the recommended method for creating pool sets is with the
**pmempool**(1) utility.

When creating a pool set consisting of multiple files, the *path* argument
passed to **pmemobj_createU**()/**pmemobj_createW**() must point to the special *set* file that defines
the pool layout and the location of all the parts of the pool set. The
*poolsize* argument must be 0. The meaning of the *layout* and *mode* arguments
does not change, except that the same *mode* is used for creation of all the
parts of the pool set.

The *set* file is a plain text file, the structure of which is described in
**poolset**(5).

The **pmemobj_openU**()/**pmemobj_openW**() function opens an existing object store memory pool.
Similar to **pmemobj_createU**()/**pmemobj_createW**(), *path* must identify either an existing
obj memory pool file, or the *set* file used to create a pool set.
If *layout* is non-NULL, it is compared to the layout
name provided to **pmemobj_createU**()/**pmemobj_createW**() when the pool was first created. This can
be used to verify that the layout of the pool matches what was expected.
The application must have permission to open the file and memory map it with
read/write permissions.

Be aware that if the pool contains bad blocks inside, opening can be aborted
by the SIGBUS signal, because currently the pool is not checked against
bad blocks during opening. It can be turned on by setting the CHECK_BAD_BLOCKS
compat feature. For details see description of this feature
in **pmempool-feature**(1).

The **pmemobj_close**() function closes the memory pool indicated by *pop* and
deletes the memory pool handle. The object store itself lives on in the file
that contains it and may be re-opened at a later time using
**pmemobj_openU**()/**pmemobj_openW**() as described above.

The **pmemobj_checkU**()/**pmemobj_checkW**() function performs a consistency check of the file
indicated by *path*. **pmemobj_checkU**()/**pmemobj_checkW**() opens the given *path* read-only so
it never makes any changes to the file. This function is not supported on
Device DAX.

# RETURN VALUE #

The **pmemobj_createU**()/**pmemobj_createW**() function returns a memory pool handle to be used with
most of the functions in **libpmemobj**(7). On error it returns NULL
and sets *errno* appropriately.

The **pmemobj_openU**()/**pmemobj_openW**() function returns a memory pool handle to be used with
most of the functions in **libpmemobj**(7). If an error prevents the pool
from being opened, or if the given *layout* does not match the pool's layout,
**pmemobj_openU**()/**pmemobj_openW**() returns NULL and sets *errno* appropriately.

The **pmemobj_close**() function returns no value.

The **pmemobj_checkU**()/**pmemobj_checkW**() function returns 1 if the memory pool is found to be
consistent. Any inconsistencies found will cause **pmemobj_checkU**()/**pmemobj_checkW**() to
return 0, in which case the use of the file with **libpmemobj**(7) will result
in undefined behavior. The debug version of **libpmemobj**(7) will provide
additional details on inconsistencies when **PMEMOBJ_LOG_LEVEL** is at least 1,
as described in the **DEBUGGING AND ERROR HANDLING** section in
**libpmemobj**(7). **pmemobj_checkU**()/**pmemobj_checkW**() returns -1 and sets *errno* if it cannot
perform the consistency check due to other errors.


# CAVEATS #

Not all file systems support **posix_fallocate**(3). **pmemobj_createU**()/**pmemobj_createW**() will
fail if the underlying file system does not support **posix_fallocate**(3).


# SEE ALSO #

**creat**(2), **msync**(2), **pmem_is_pmem**(3), **pmem_persist**(3),
**posix_fallocate**(3), **libpmem**(7), **libpmemobj**(7)
and **<http://pmem.io>**

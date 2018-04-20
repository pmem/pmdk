---
layout: manual
Content-Style: 'text/css'
title: PMEMCTO_OPEN
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

[comment]: <> (pmemcto_open.3 -- man page for libpmemcto)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[ERRORS](#errors)<br />
[CAVEATS](#caveats)<br />
[BUGS](#bugs)<br />
[SEE ALSO](#see-also)<br />


# NAME #

**pmemcto_create**(), **pmemcto_open**(),
**pmemcto_close**(), **pmemcto_check**()
-- create, open, close and validate close-to-open persistence pool


# SYNOPSIS #

```c
#include <libpmemcto.h>

PMEMctopool *pmemcto_create(const char *path, const char *layout,
		size_t poolsize, mode_t mode);
PMEMctopool *pmemcto_open(const char *path, const char *layout);
void pmemcto_close(PMEMctopool *pcp);
int pmemcto_check(const char *path, const char *layout);
```




# DESCRIPTION #

The **pmemcto_create**() function creates a close-to-open persistence pool with
the given total *poolsize*.  The resulting pool is then used with
functions like **pmemcto_malloc**(3) and **pmemcto_free**(3) to provide the
familiar *malloc-like* programming model for the memory pool.
*path* specifies the name of the memory pool file to be
created. *layout* specifies the application's layout type in the form of a
string. The layout name is not interpreted by **libpmemcto**(7), but may be
used as a check when **pmemcto_open**() is called. The layout name, including
the terminating null byte ('\\0'), cannot be longer than **PMEMCTO_MAX_LAYOUT**
as defined in **\<libpmemcto.h\>**. A NULL *layout* is equivalent
to using an empty string as a layout name. *mode* specifies the permissions to
use when creating the file, as described by **creat**(2). The memory pool file
is fully allocated to the size *poolsize* using **posix_fallocate**(3). The
caller may choose to take responsibility for creating the memory pool file
by creating it before calling **pmemcto_create**(), and then specifying
*poolsize* as zero. In this case **pmemcto_create**() will take the pool size
from the size of the existing file and will verify that the file appears to be
empty by searching for any non-zero data in the pool header at the beginning of
the file. The minimum net pool size allowed by the library for a local
close-to-open persistence pool is defined in **\<libpmemcto.h\>** as
**PMEMCTO_MIN_POOL**.

Depending on the configuration of the system, the available non-volatile
memory space may be divided into multiple memory devices. In such case, the
maximum size of the pmemcto memory pool could be limited by the capacity of a
single memory device. **libpmemcto**(7) allows building a close-to-open
persistence pool spanning multiple memory devices by creation of persistent
memory pools consisting of multiple files, where each part of such a *pool set*
may be stored on a different memory device or pmem-aware filesystem.

Creation of all the parts of the pool set can be done with **pmemcto_create**();
however, the recommended method for creating pool sets is by using the
**pmempool**(1) utility.

When creating a pool set consisting of multiple files, the *path* argument
passed to **pmemcto_create**() must point to the special *set* file that defines
the pool layout and the location of all the parts of the pool set. The
*poolsize* argument must be 0. The meaning of the *layout* and *mode* arguments
does not change, except that the same *mode* is used for creation of all the
parts of the pool set.

For more information on pool set format, see **poolset**(5).

The **pmemcto_open**() function opens an existing close-to-open persistence
memory pool.
*path* must be an existing file containing a pmemcto memory pool as created
by **pmemcto_create**(). If *layout* is non-NULL, it is compared to the layout
name provided to **pmemcto_create**() when the pool was first created. This can
be used to verify that the layout of the pool matches what was expected.
The application must have permission to open the file and memory map it with
read/write permissions.

The **pmemcto_close**() function closes the memory pool indicated by *pcp*
and deletes the memory pool handle.  The close-to-open memory pool itself
lives on in the file that contains it and may be re-opened at a later time
using **pmemcto_open**() as described above.
If the pool was not closed gracefully due to abnormal program
termination or power failure, the pool is in an inconsistent state
causing subsequent pool opening to fail.

The **pmemcto_check**() function performs a consistency check of the file
indicated by *path*, and returns 1 if the memory pool is found to be consistent.
If the pool is found not to be consistent, further use of the
file with **libpmemcto**(7) will result in undefined behavior.
The debug version of **libpmemcto**(7) will provide additional details
on inconsistencies when **PMEMCTO_LOG_LEVEL** is at least 1, as described
in the **DEBUGGING AND ERROR HANDLING** section of **libpmemcto**(7).
**pmemcto_check**() will return -1 and set *errno* if it cannot perform
the consistency check due to other errors. **pmemcto_check**() opens
the given *path* read-only so it never makes any changes to the file.
This function is not supported on Device DAX.


# RETURN VALUE #

On success, **pmemcto_create**() returns a *PMEMctopool\** handle to the
close-to-open persistence memory pool. On error, it returns NULL and sets
*errno* appropriately.

On success, **pmemcto_open**() returns a *PMEMctopool\** handle that can be
used with most of the functions in **libpmemcto**(7). On error, it returns
NULL and sets *errno* appropriately.

The **pmemcto_close**() function returns no value.

**pmemcto_check**() returns 1 if the memory pool is found to be consistent.
If the check is successfully performed but the pool is found to be inconsistent,
**pmemcto_check**() returns 0.  If the consistency check cannot be performed,
**pmemcto_check**() returns -1 and sets *errno* appropriately.
This includes the case where *layout* is non-NULL and does not match
the layout string given when the pool was created.


# ERRORS #

**EINVAL** "layout" string does not match the layout stored in pool header.

**EINVAL** "layout" string is longer than **PMEMCTO_MAX_LAYOUT**.

**EINVAL** *poolsize* is less than **PMEMCTO_MIN_POOL**.

**EINVAL** Invalid format of the pool set file.

**EINVAL** Invalid pool header.

**EEXIST** *path* passed to **pmemcto_create**() points to a pool set file,
  but *poolsize* is not zero.

**EEXIST** *path* passed to **pmemcto_create**() points to an existing file,
  but *poolsize* is not zero.

**EEXIST** *path* passed to **pmemcto_create**() points to an existing file,
  which is not-empty.

**EAGAIN** The pmemcto pool pointed by *path* is already open.

**EACCES** No write access permission to the pool file(s).

**ENOMEM** The pool cannot be mapped at the address it was created.


# CAVEATS #

Not all file systems support **posix_fallocate**(3). **pmemcto_create**() will
fail if the underlying file system does not support **posix_fallocate**(3).


# BUGS #

Unlike **libpmemobj**(7), data replication is not supported in
**libpmemcto**(7).
Thus, it is not allowed to specify replica sections in pool set files.


# SEE ALSO #

**ndctl-create-namespace**(1), **pmempool-create**(1),
**jemalloc**(3), **poolset**(5),
**libpmemcto**(7), **libpmemobj**(7) and **<http://pmem.io>**

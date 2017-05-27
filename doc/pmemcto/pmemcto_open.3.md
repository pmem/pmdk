---
layout: manual
Content-Style: 'text/css'
title: PMEMCTO_OPEN(3)
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

[comment]: <> (pmemcto_open.3 -- man page for libpmemcto)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[ERRORS](#errors)<br />
[BUGS](#bugs)<br />
[ACKNOWLEDGEMENTS](#acknowledgements)<br />
[SEE ALSO](#see-also)<br />


# NAME #

pmemcto_open, pmemcto_create, pmemcto_close, pmemcto_check -- create, open
or check consistency of a close-to-open persistence memory pool


# SYNOPSIS #

```c
#include <libpmemcto.h>

PMEMctopool *pmemcto_open(const char *path, const char *layout);
PMEMctopool *pmemcto_create(const char *path, const char *layout,
		size_t poolsize, mode_t mode);
void pmemcto_close(PMEMctopool *pcp);

int pmemcto_check(const char *path, const char *layout);
```

# DESCRIPTION #

The **pmemobj_open**() function opens an existing close-to-open persistence
memory pool, returning a memory pool handle used with most of
the **libpmemcto** functions.  *path* must be an existing file containing
a pmemcto memory pool as created by **pmemcto_create**().  If *layout*
is non-NULL, it is compared to the layout name provided to **pmemcto_create**()
when the pool was first created.  This can be used to verify the layout of
the pool matches what was expected.  The application must have permission
to open the file and memory map it with read/write permissions.  If an error
prevents the pool from being opened, or if the given *layout* does not match
the pool's layout, **pmemcto_open**() returns NULL and sets *errno*
appropriately.


The **pmemcto_create**() function creates a close-to-open persistence pool
with the given total *poolsize*.  The resulting pool is then used with
functions like **pmemcto_malloc**(3) and **pmemcto_free**(3) to provide the
familiar *malloc-like* programming model for the memory pool.
*path* specifies the name of the memory pool file to be created.
*layout* specifies the application's layout type in the form of a string.
The layout name is not interpreted by **libpmemcto**, but may be used as
a check when **pmemcto_open**() is called.  The layout name, including the
terminating null byte ('\0'), cannot be longer than **PMEMCTO_MAX_LAYOUT**
as defined in **\<libpmemcto.h\>**.  It is allowed to pass NULL as *layout*,
which is equivalent for using an empty string as a layout name.
*mode* specifies the permissions to use when creating the file as described
by **creat**(2).
The memory pool file is fully allocated to the size *poolsize* using
**posix_fallocate**(3).  The caller may choose to take responsibility for
creating the memory pool file by creating it before calling **pmemcto_create**()
and then specifying *poolsize* as zero.  In this case **pmemcto_create**()
will take the pool size from the size of the existing file and will verify that
the file appears to be empty by searching for any non-zero data in the pool
header at the beginning of the file.  The minimum file size allowed by the
library for a local close-to-open memory pool is defined in **\<libpmemcto.h\>**
as **PMEMCTO_MIN_POOL**.

Depending on the configuration of the system, the available space of
non-volatile memory space may be divided into multiple memory devices.
In such case, the maximum size of the pmemcto memory pool could be limited
by the capacity of a single memory device.  The **libpmemcto** allows building
close-to-open memory pool spanning multiple memory devices by creation
of persistent memory pools consisting of multiple files, where each part
of such a *pool set* may be stored on different pmem-aware filesystem.

Creation of all the parts of the pool set can be done with the
**pmemcto_create**() function.  However, the recommended method for creating
pool sets is to do it by using the **pmempool**(1) utility.

When creating the pool set consisting of multiple files, the *path* argument
passed to **pmemcto_create**() must point to the special *set* file that defines
the pool layout and the location of all the parts of the pool set.
The *poolsize* argument must be 0.  The meaning of *layout* and *mode*
arguments doesn't change, except that the same *mode* is used for creation
of all the parts of the pool set.  If the error prevents any of the pool set
files from being created, **pmemblk_create**() returns NULL and sets *errno*
appropriately.

When opening the pool set consisting of multiple files, the *path* argument
passed to **pmemcto_open**() must not point to the pmemcto memory pool file,
but to the same *set* file that was used for the pool set creation.  If an error
prevents any of the pool set files from being opened, or if the actual size
of any file does not match the corresponding part size defined in *set* file
**pmemcto_open**() returns NULL and sets *errno* appropriately.

The set file is a plain text file, which must start with the line containing
a *PMEMPOOLSET* string, followed by the specification of all the pool parts
in the next lines.  For each part, the file size and the absolute path must
be provided.

The size has to be compliant with the format specified in IEC 80000-13,
IEEE 1541 or the Metric Interchange Format. Standards accept SI units with
obligatory B - kB, MB, GB, ... (multiplier by 1000) and IEC units with optional
"iB" - KiB, MiB, GiB, ..., K, M, G, ... - (multiplier by 1024).

The path of a part can point to a Device DAX and in such case the size
argument can be set to an "AUTO" string, which means that the size of the
device will be automatically resolved at pool creation time.
When using Device DAX there's also one additional restriction - it is not
allowed to concatenate more than one Device DAX device in a single pool set
if the configured internal alignment is other than 4KiB.  In such case a pool
set can consist only of a single part (single Device DAX).
Please see **ndctl-create-namespace**(1) for information on how to configure
desired alignment on Device DAX.

Device DAX is the device-centric analogue of Filesystem DAX. It allows memory
ranges to be allocated and mapped without need of an intervening file system.
For more information please see **ndctl-create-namespace**(1).

The minimum file size of each part of the pool set is the same as the minimum
size allowed for a block pool consisting of one file. It is defined in
**\<libpmemcto.h\>** as **PMEMCTO_MIN_POOL**.  Lines starting with "#"
character are ignored.

Here is the example "myctopool.set" file:

```
PMEMPOOLSET
100G /mountpoint0/myfile.part0
200G /mountpoint1/myfile.part1
400G /mountpoint2/myfile.part2
```

The files in the set may be created by running the following command:

```
$ pmempool create cto -l "mylayout" myctopool.set
```


The **pmemcto_close**() function closes the memory pool indicated by *pcp*
and deletes the memory pool handle.  The close-to-open memory pool itself
lives on in the file that contains it and may be re-opened at a later time
using **pmemcto_open**() as described above.
If the pool was not closed gracefully due to abnormal program
termination or power failure, the pool is in an inconsistent state
causing subsequent pool opening to fail.


The **pmemcto_check**() function performs a consistency check of the file
indicated by *path* and returns 1 if the memory pool is found to be consistent.
Any inconsistencies found will cause **pmemcto_check**() to return 0, in which
case the use of the file with **libpmemcto** will result in undefined behavior.
The debug version of **libpmemcto** will provide additional details
on inconsistencies when **PMEMCTO_LOG_LEVEL** is at least 1, as described
in the **DEBUGGING AND ERROR HANDLING** section of **pmemcto**(3).
**pmemcto_check**() will return -1 and set *errno* if it cannot perform
the consistency check due to other errors.  **pmemcto_check**() opens
the given *path* read-only so it never makes any changes to the file.
This function is not supported on Device DAX.


# RETURN VALUE #

On success, the **pmemcto_open**() and **pmemcto_create**() functions return
a memory pool handle, or NULL if an error occured (in which case *errno*
is set appropriately).

The **pmemcto_check**() returns 1 if the memory pool is found to be consistent,
and 0 otherwise.  If it cannot perform the consistency check -1 is returned.


# ERRORS #

**EINVAL** "layout" string does not match the layout stored in pool header.

**EINVAL** "layout" string is longer than **PMEMCTO_MAX_LAYOUT**.

**EINVAL** *poolsize* is less than **PMEMCTO_MIN_POOL**.

**EINVAL** *path* passed to **pmemcto_create**() points to a pool set file,
  but *poolsize* is not zero.

**EINVAL** *path* passed to **pmemcto_create**() points to an existing file,
  but *poolsize* is not zero.

**EINVAL** *path* passed to **pmemcto_create**() points to an existing file,
  which is not-empty.

**EINVAL** Invalid format of the pool set file.

**EINVAL** Invalid pool header.

**EAGAIN** The pmemcto pool pointed by *path* is already open.

**EACCES** No write access permission to the pool file(s).


# BUGS #

Unlike **libpmemobj**(3), data replication is not supported in **libpmemcto**.
Thus, it is not allowed to specify replica sections in pool set files.


# ACKNOWLEDGEMENTS #

**libpmemcto** depends on jemalloc, written by Jason Evans, to do the heavy
lifting of managing dynamic memory allocation. See:
<http://www.canonware.com/jemalloc>

**libpmemcto** builds on the persistent memory programming model recommended
by the SNIA NVM Programming Technical Work Group:
<http://snia.org/nvmp>


# SEE ALSO #

**pmemcto**(3), **jemalloc**(3),
**pmempool-create**(1), **ndctl-create-namespace**(1)
and **<http://pmem.io>**

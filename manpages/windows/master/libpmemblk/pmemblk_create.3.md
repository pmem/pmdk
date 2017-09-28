---
layout: manual
Content-Style: 'text/css'
title: PMEMBLK_CREATE
collection: libpmemblk
header: NVM Library
date: pmemblk API version 1.0
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

[comment]: <> (pmemblk_create.3 -- man page for most commonly used functions from libpmemblk library)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />


# NAME #

**pmemblk_createU**()/**pmemblk_createW**(), **pmemblk_openU**()/**pmemblk_openW**(),
**pmemblk_close**() -- create, open and close block pool


# SYNOPSIS #

```c
#include <libpmemblk.h>

PMEMblkpool *pmemblk_createU(const char *path, size_t bsize,
		size_t poolsize, mode_t mode);
PMEMblkpool *pmemblk_createW(const wchar_t *path, size_t bsize,
		size_t poolsize, mode_t mode);
PMEMblkpool *pmemblk_openU(const char *path, size_t bsize);
PMEMblkpool *pmemblk_openW(const wchar_t *path, size_t bsize);
```

>NOTE: NVML API supports UNICODE. If **NVML_UTF8_API** macro is defined then
basic API functions are expanded to UTF-8 API with postfix *U*,
otherwise they are expanded to UNICODE API with postfix *W*.


# DESCRIPTION #

The **pmemblk_createU**()/**pmemblk_createW**() function creates a block memory pool with the given total
*poolsize* divided up into as many elements of size *bsize* as will fit in the pool.
Since the transactional nature of a block memory pool requires some space overhead
in the memory pool, the resulting number of available blocks is less than
*poolsize*/*bsize*, and is made available to the caller via the **pmemblk_nblock**(3)
function. Given the specifics of the implementation, the number
of available blocks for the user cannot be less than 256. This translates to
at least 512 internal blocks. *path* specifies the name of the memory pool file
to be created. *mode* specifies the permissions to use when creating the file
as described by **creat**(2). The memory pool file is fully allocated to the size
*poolsize* using **posix_fallocate**(3). The caller may choose to take
responsibility for creating the memory pool file by creating it before calling
**pmemblk_createU**()/**pmemblk_createW**() and then specifying *poolsize* as zero. In this case
**pmemblk_createU**()/**pmemblk_createW**() will take the pool size from the size of the existing file
and will verify that the file appears to be empty by searching for any non-zero
data in the pool header at the beginning of the file. The net pool size of a
pool file is equal to the file size. The minimum net pool size allowed by the
library for a block pool is defined in **\<libpmemblk.h\>** as **PMEMBLK_MIN_POOL**.
*bsize* can be any non-zero value, however **libpmemblk** will silently round up
the given size to **PMEMBLK_MIN_BLK**, as defined in **\<libpmemblk.h\>**.

Depending on the configuration of the system, the available space of non-volatile
memory space may be divided into multiple memory devices. In such case, the maximum
size of the pmemblk memory pool could be limited by the capacity of a single memory
device. The **libpmemblk**(7) allows building persistent memory resident array spanning
multiple memory devices by creation of persistent memory pools consisting of multiple
files, where each part of such a *pool set* may be stored on different pmem-aware filesystem.

Creation of all the parts of the pool set can be done with the **pmemblk_createU**()/**pmemblk_createW**()
function. However, the recommended method for creating pool sets is to do it by
using the **pmempool**(1) utility.

When creating the pool set consisting of multiple files, the *path* argument passed
to **pmemblk_createU**()/**pmemblk_createW**() must point to the special *set* file that defines the pool
layout and the location of all the parts of the pool set. The *poolsize* argument
must be 0. The meaning of *layout* and *mode* arguments doesn't change, except that
the same *mode* is used for creation of all the parts of the pool set.

Poolset file format in more details is describe in **poolset**(5).

The **pmemblk_openU**()/**pmemblk_openW**() function opens an existing block memory pool.
The *path* argument must be an existing file containing a block memory pool
as created by **pmemblk_createU**()/**pmemblk_createW**(). The application must have permission to open the file
and memory map it with read/write permissions. If the *bsize* provided is
non-zero, **pmemblk_openU**()/**pmemblk_openW**() will verify the given block size matches the block
size used when the pool was created. Otherwise, **pmemblk_openU**()/**pmemblk_openW**() will open
the pool without verification of the block size. The *bsize* can be determined
using the **pmemblk_bsize**(3) function.
When opening the pool set consisting of multiple files, the *path* argument passed
to **pmemblk_openU**()/**pmemblk_openW**() must not point to the pmemblk memory pool file, but to the same
*set* file that was used for the pool set creation.

The **pmemblk_close**() function closes the memory pool
indicated by *pbp* and deletes the memory pool handle.
The block memory pool itself lives on in the file that contains it
and may be re-opened at a later time using **pmemblk_openU**()/**pmemblk_openW**() as described above.


# RETURN VALUE #

The **pmemblk_createU**()/**pmemblk_createW**() function returns pointer to block memory pool or returns
NULL and sets *errno* appropriately if the error prevents any of the
pool set files from being created.

The **pmemblk_openU**()/**pmemblk_openW**() function returns a memory pool handle
used with most of the functions in **pmemblk**(7) library.
If an error prevents the pool from being
opened, **pmemblk_openU**()/**pmemblk_openW**() returns NULL and sets *errno* appropriately.
A block size mismatch with the *bsize* argument passed in results in *errno*
being set to **EINVAL**.
If an error prevents any of the
pool set files from being opened, or if the actual size of any file does not match
the corresponding part size defined in *set* file **pmemblk_openU**()/**pmemblk_openW**() returns NULL
and sets *errno* appropriately.

The **pmemblk_close**() function returns no value.


# SEE ALSO #
**pmempool**(1), **posix_fallocate**(2), **pmemblk**(7) and **<http://pmem.io>**

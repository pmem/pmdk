---
layout: manual
Content-Style: 'text/css'
title: PMEMLOG_CREATE!3
header: NVM Library
date: pmemlog API version 1.0
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

[comment]: <> (pmemlog_create.3 -- man page for most commonly used functions from libpmemlog library)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />


# NAME #

!pmemlog_create, !pmemlog_open,
**pmemlog_close**() -- create, open and close log pool


# SYNOPSIS #

```c
#include <libpmemlog.h>

!ifdef{WIN32}
{
PMEMlogpool *pmemlog_openU(const char *path);
PMEMlogpool *pmemlog_openW(const wchar_t *path);
PMEMlogpool *pmemlog_createU(const char *path, size_t poolsize, mode_t mode);
PMEMlogpool *pmemlog_createW(const wchar_t *path, size_t poolsize, mode_t mode);
}{
PMEMlogpool *pmemlog_open(const char *path);
PMEMlogpool *pmemlog_create(const char *path, size_t poolsize, mode_t mode);
}
void pmemlog_close(PMEMlogpool *plp);
```

!ifdef{WIN32}
{
>NOTE: NVML API supports UNICODE. If **NVML_UTF8_API** macro is defined then
basic API functions are expanded to UTF-8 API with postfix *U*,
otherwise they are expanded to UNICODE API with postfix *W*.
}


# DESCRIPTION #

The !pmemlog_create function creates a log memory pool with the given total *poolsize*.
Since the transactional nature of a log memory pool requires some
space overhead in the memory pool, the resulting available log size is
less than *poolsize*, and is made available to the caller via the **pmemlog_nbyte**(3) function.
*path* specifies the name of the memory pool file to be created.
*mode* specifies the permissions to use when creating the file as
described by **creat**(2). The memory pool file is fully allocated
to the size *poolsize* using **posix_fallocate**(3).
The caller may choose to take responsibility for creating the memory pool file
by creating it before calling !pmemlog_create and then specifying *poolsize* as zero.
In this case !pmemlog_create will take the pool size from the size of the existing file
and will verify that the file appears to be empty by searching for any non-zero
data in the pool header at the beginning of the file.
The net pool size of a pool file is equal to the file size.
The minimum net pool size allowed by the library for a log pool
is defined in **\<libpmemlog.h\>** as **PMEMLOG_MIN_POOL**.

Depending on the configuration of the system, the available space
of non-volatile memory space may be divided into multiple memory devices.
In such case, the maximum size of the pmemlog memory pool
could be limited by the capacity of a single memory device.
The **libpmemlog**(7) allows building persistent memory
resident log spanning multiple memory devices by creation of
persistent memory pools consisting of multiple files,
where each part of such a *pool set* may be
stored on different pmem-aware filesystem.

Creation of all the parts of the pool set can be done with the !pmemlog_create function. However,
the recommended method for creating pool sets is to do it by using the **pmempool**(1) utility.

When creating the pool set consisting of multiple files,
the *path* argument passed to !pmemlog_create must point to the special *set* file that defines
the pool layout and the location of all the parts of the pool set.
The *poolsize* argument must be 0. The meaning of *layout* and *mode* arguments doesn't
change, except that the same *mode* is used for creation of all the parts of the pool set.

The set file is a plain text file, which structure is described in **poolset**(5) man page.

The !pmemlog_open function opens an existing log memory pool.
Argument *path* must be an existing file containing a log memory pool
as created by !pmemlog_create. The application must have permission
to open the file and memory map it with read/write permissions.

When opening the pool set consisting of multiple files, the *path* argument
passed to !pmemlog_open must not point to the pmemlog memory pool file, but to
the same *set* file that was used for the pool set creation.

The **pmemlog_close**() function closes the memory pool indicated by *plp*
and deletes the memory pool handle. The log memory pool itself lives on in the file
that contains it and may be re-opened at a later time using !pmemlog_open as described above.

# RETURN VALUE #

The **pmemlog_create**() function returns NULL and sets *errno*
appropriately if the error prevents any of the pool set files from
being created, othervise it returns a memory pool handle.

The **pmemlog_open**() function returns NULL and sets *errno*
appropriately if an error prevents the pool from being opened.
If no errors occurs, function returns a memory pool handle
used with most of the functions from **libpmemlog**(7).
If an error prevents any of the pool set files from being opened,
or if the actual size of any file does not match the corresponding part size
defined in *set* file !pmemlog_open returns NULL and sets *errno* appropriately.

The **pmemlog_close**() function returns no value.


# SEE ALSO #

**pmempool**(1), **creat**(2), **posix_fallocate**(2),
**pmemlog_nbyte**(3), **poolset**(5), **libpmemlog**(7)
and **<http://pmem.io>**

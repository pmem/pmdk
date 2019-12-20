---
layout: manual
Content-Style: 'text/css'
title: _MP(PMEM2\_CONFIG\_SET\_FD, 3)
collection: libpmem2
header: PMDK
date: pmem2 API version 1.0
...

[comment]: <> (Copyright 2019, Intel Corporation)

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

[comment]: <> (pmem2_config_set_fd.3 -- man page for pmem2_config_set_fd

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[ERRORS](#errors)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmem2_config_set_fd**(), **pmem2_config_set_handle**() - stores a file descriptor in pmem2_config

# SYNOPSIS #

```c
#include <libpmem2.h>

struct pmem2_config;
int pmem2_config_set_fd(struct pmem2_config *config, int fd);
int pmem2_config_set_handle(struct pmem2_config *config, HANDLE handle); /* Windows only */
```

# DESCRIPTION #

On Linux the **pmem2_config_set_fd**() function validates and stores a file descriptor in pmem2_config.

On Windows the **pmem2_config_set_fd**() function converts a file descriptor to a file handle (using **\_get_osfhandle**()), and passes it to **pmem2_config_set_handle**().
By default **\_get_osfhandle**() calls abort() in case of invalid file descriptor, but this behavior can be suppressed by **\_set_abort_behavior**() and **SetErrorMode**() functions.
Please check MSDN documentation for more information about Windows CRT error handling.

*fd* must be opened with *O_RDONLY* or *O_RDWR* mode, but on Windows it is not validated.

If *fd* is negative, then file descriptor (or handle on Windows) in *config is set to default, uninitialized value.

The **pmem2_config_set_handle**() function validates and stores a file handle in pmem2_config.
If *handle* is INVALID_HANDLE_VALUE, file descriptor (or handle on Windows) in *config is set to default, uninitialized value.

# RETURN VALUE #

**pmem2_config_set_fd**() and **pmem2_config_set_handle**() functions return 0 on success or one of the error values listed in the next section.

# ERRORS #

The **pmem2_config_set_fd**() function can return the following errors:

 * **PMEM2\_E\_INVALID\_FILE\_HANDLE** - *fd* is not an open file descriptor. On Windows the function can **abort**() on this failure based on CRT's abort() behavior.

 * **PMEM2\_E\_INVALID\_FILE\_HANDLE** - *fd* is opened in O_WRONLY mode.

On Linux:

 * **PMEM2\_E\_INVALID\_FILE\_TYPE** - *fd* points to a directory, block device, pipe, or socket.

 * **PMEM2\_E\_INVALID\_FILE\_TYPE** - *fd* points to a character device other than Device DAX.

On Windows:

 * **PMEM2\_E\_INVALID\_FILE\_TYPE** - *handle* points to a resource that is not a regular file.

On Windows **pmem2_config_set_fd**() can return all errors from the underlying **pmem2_config_set_handle**() function.

The **pmem2_config_set_handle**() can return the following errors:

 * **PMEM2\_E\_INVALID\_FILE\_HANDLE** - *handle* points to a resource that is not a file.

 * **PMEM2\_E\_INVALID\_FILE\_TYPE** - *handle* points to a directory.

# SEE ALSO #

**errno**(3), **pmem2_map**(3), **libpmem2**(7)
and **<http://pmem.io>**

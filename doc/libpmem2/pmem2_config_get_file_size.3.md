---
layout: manual
Content-Style: 'text/css'
title: _MP(PMEM2\_CONFIG\_GET\_FILE\_SIZE, 3)
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

[comment]: <> (pmem2_config_get_file_size.3 -- man page for pmem2_config_get_file_size)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmem2_config_get_file_size**() - query a file size

# SYNOPSIS #

```c
#include <libpmem2.h>

struct pmem2_config;
int pmem2_config_get_file_size(const struct pmem2_config *config, size_t *size);
```

# DESCRIPTION #

The **pmem2_config_get_file_size**() function retrieves the size of the file
in bytes pointed by file descriptor or handle stored in the *config* and puts
it in *\*size*.

This function is a portable replacement for OS-specific APIs.
On Linux, it hides the quirkiness of Device DAX size detection.

# RETURN VALUE #

The **pmem2_config_get_file_size**() function returns 0 on success.
If the function fails, the *\*size* variable is left unmodified, and one of
the following errors is returned:

On all systems:

* **PMEM2\_E\_FILE\_HANDLE\_NOT\_SET** - config doesn't contain the file handle
(see **pmem2_config_set_fd**(3), **pmem2_config_set_handle**(3)).

* **PMEM2\_E\_INVALID\_FILE\_HANDLE** - config contains an invalid file handle.

On Windows:

* **PMEM2\_E\_INVALID\_FILE\_TYPE** - handle points to a resource that is not
a regular file.

On Linux:

* **PMEM2\_E\_INVALID\_FILE\_TYPE** - file descriptor points to a directory,
block device, pipe, or socket.

* **PMEM2\_E\_INVALID\_FILE\_TYPE** - file descriptor points to a character
device other than Device DAX.

* -**errno** set by failing **fstat**(2), while trying to validate the file
descriptor.

* -**errno** set by failing **realpath**(3), while trying to determine whether
fd points to a Device DAX.

* -**errno** set by failing **open**(2), while trying to determine Device DAX's
size.

* -**errno** set by failing **strtoull**(3), while trying to determine
Device DAX's size.

On FreeBSD:

* **PMEM2\_E\_INVALID\_FILE\_TYPE** - file descriptor points to a directory,
block device, pipe, socket, or character device.

* -**errno** set by failing **fstat**(2), while trying to validate the file
descriptor.

# SEE ALSO #

**errno**(3),  **fstat**(2), **realpath**(3), **open**(2), **strtoull**(3),
**pmem2_config_new**(3), **pmem2_config_set_handle**(3),
**pmem2_config_set_fd**(3), **libpmem2**(7) and **<http://pmem.io>**

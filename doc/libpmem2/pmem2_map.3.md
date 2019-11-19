---
layout: manual
Content-Style: 'text/css'
title: _MP(PMEM2_MAP, 3)
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

[comment]: <> (pmem2_map.3 -- man page for libpmem2 pmem2_map operation)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmem2_map**() - creates a mapping

# SYNOPSIS #

```c
#include <libpmem2.h>

int pmem2_map(const struct pmem2_config *config, struct pmem2_map **map_ptr);
```

# DESCRIPTION #

The **pmem2_map**() function creates a new mapping in the virtual address space
of the calling process. This function requires that following arguments are set
in the *config* object:

* The source of the mapping, which may be either an opened file descriptor or
an opened handle to a file.

    * The file descriptor has to be opened using *O_RDONLY* or *O_RDWR* file
access mode and passed via **pmem2_config_set_fd**(3). Note on Windows the
provided file descriptor is converted to a handle using **\_get_osfhandle**().
For details please see the **open**(3), **pmem2_config_set_fd**(3) and
**\_get_osfhandle**() manual pages.

    * Similarly, the handle has to be created with an access mode of
*GENERIC_READ* or *(GENERIC_READ | GENERIC_WRITE)* and passed via
**pmem2_config_set_handle**(3). For details please see the **CreateFile**() and
**pmem2_config_set_handle**(3) manual pages. (Windows only)

    The above functions are mutually exclusive, and only the last source set
prior to **pmem2_map**() is used.

If the **pmem2_map**() function succeeds in creating a new mapping it
instantiates a new *struct pmem2_map** object describing the mapping. The
pointer to this newly created object is stored in the user-provided variable
passed via the *map_ptr* pointer. If the mapping fails the variable pointed by
*map_ptr* will contain a NULL value and appropriate error value will be
returned. For a list of possible return values please see
[RETURN VALUE](#return-value).

All *struct pmem2_map* objects created via the **pmem2_map**() function have to
be destroyed using the **pmem2_unmap**() function. For details please see
**pmem2_unmap**(3) manual page.

# RETURN VALUE #

When **pmem2_map**() succeeds it returns **PMEM2_E_OK**. Otherwise, it returns
one of the following error values:

* **PMEM2_E_INVALID_FILE_HANDLE** - invalid *file descriptor* value in *config*

* **PMEM2_E_INVALID_FILE_TYPE** - cannot determine a file type or it is not
supported e.g. it is a directory

* **PMEM2_E_MAP_RANGE** - *offset* + *length* is too big to represent it using
*size_t* data type

* **PMEM2_E_MAP_RANGE** - end of the mapping (*offset* + *length*) is outside
of the file. The file is too small.

* **PMEM2_E_MAPPING_EXISTS** - if the object exists before the function call.
For details please see **CreateFileMappingA**() manual pages. (Windows only)

It can also return **-EACCES**, **-EAGAIN**, **-EBADF**, **-ENFILE**,
**-ENODEV**, **-ENOMEM**, **-EPERM**, **-ETXTBSY** from the underlying
**mmap**(2) function. It is used with and without **MAP_ANONYMOUS**.

**-EACCES** may be returned only if the file descriptor points to an
append-only file.

It can also return all errors from the underlying
**pmem2_config_get_file_size**() function.

# SEE ALSO #

**pmem2_unmap**(3), **pmem2_config_set_fd**(3),
**pmem2_config_get_file_size**(3), **libpmem2**(7), **mmap**(2), **open**(3) and
**<http://pmem.io>**

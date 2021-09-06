---
layout: manual
Content-Style: 'text/css'
title: _MP(PMEM2_SOURCE_FROM_FD, 3)
collection: libpmem2
header: PMDK
date: pmem2 API version 1.0
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2019-2020, Intel Corporation)

[comment]: <> (pmem2_source_from_fd.3 -- man page for pmem2_source_from_fd

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[ERRORS](#errors)<br />
[CAVEATS](#caveats)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmem2_source_from_fd**(), **pmem2_source_from_handle**(),
**pmem2_source_delete**() - creates or deletes an instance of persistent memory
data source

# SYNOPSIS #

```c
#include <libpmem2.h>

int pmem2_source_from_fd(struct pmem2_source *src, int fd);
int pmem2_source_from_handle(struct pmem2_source *src, HANDLE handle); /* Windows only */
int pmem2_source_delete(struct pmem2_source **src);
```

# DESCRIPTION #

On Linux the **pmem2_source_from_fd**() function validates the file descriptor
and instantiates a new *struct pmem2_source** object describing the data source.

On Windows the **pmem2_source_from_fd**() function converts a file descriptor to a file handle (using **_get_osfhandle**()), and passes
it to **pmem2_source_from_handle**().
By default **_get_osfhandle**() calls abort() in case of invalid file descriptor,
but this behavior can be suppressed by **_set_abort_behavior**() and **SetErrorMode**()
functions.
Please check MSDN documentation for more information about Windows CRT error handling.

*fd* must be opened with *O_RDONLY* or *O_RDWR* mode, but on Windows it is not
validated.

If *fd* is invalid, then the function fails.

The **pmem2_source_from_handle**() function validates the handle and instantiates
a new *struct pmem2_source** object describing the data source.
If *handle* is *INVALID_HANDLE_VALUE*, then the function fails.
The handle has to be created with an access mode of *GENERIC_READ* or
*(GENERIC_READ | GENERIC_WRITE)*. For details please see the **CreateFile**()
documentation.

The **pmem2_source_delete**() function frees *\*src* returned by **pmem2_source_from_fd**() or **pmem2_source_from_handle**() and sets *\*src* to NULL. If *\*src* is NULL, no operation is performed.

# RETURN VALUE #

The **pmem2_source_from_fd**() and **pmem2_source_from_handle**() functions return 0 on success
or a negative error code on failure.

The **pmem2_source_delete**() function always returns 0.

# ERRORS #

The **pmem2_source_from_[fd|handle]**() function can fail with the following errors:

 * **PMEM2_E_INVALID_FILE_HANDLE** - *fd* is not an open and valid file descriptor. On Windows the function can **abort**() on this failure based on CRT's abort() behavior.

 * **PMEM2_E_INVALID_FILE_HANDLE** - *fd* is opened in O_WRONLY mode.

On Linux:

 * **PMEM2_E_INVALID_FILE_TYPE** - *fd* points to a directory, block device, pipe, or socket.

 * **PMEM2_E_INVALID_FILE_TYPE** - *fd* points to a character device other than Device DAX.

On Windows:

 * **PMEM2_E_INVALID_FILE_TYPE** - *handle* points to a resource that is not a regular file.

On Windows **pmem2_source_from_fd**() can return all errors from the underlying **pmem2_source_from_handle**() function.

The **pmem2_source_from_handle**() can return the following errors:

 * **PMEM2_E_INVALID_FILE_HANDLE** - *handle* points to a resource that is not a file.

 * **PMEM2_E_INVALID_FILE_TYPE** - *handle* points to a directory.

The **pmem2_source_from_fd**() and **pmem2_source_from_handle**() functions can
also return **-ENOMEM** in case of insufficient memory to
allocate an instance of *struct pmem2_source*.

# CAVEATS #

On non-DAX Windows volumes, *fd*/*handle* must remain open while the mapping
is in use.

# SEE ALSO #
**errno**(3), **pmem2_map_new**(3), **libpmem2**(7)
and **<http://pmem.io>**

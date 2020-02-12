---
layout: manual
Content-Style: 'text/css'
title: PMEM2_CONFIG_SET_FD
collection: libpmem2
header: PMDK
date: pmem2 API version 1.0
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2019, Intel Corporation)

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

int pmem2_config_set_fd(struct pmem2_config *cfg, int fd);
int pmem2_config_set_handle(struct pmem2_config *cfg, HANDLE handle); /* Windows only */
```

# DESCRIPTION #

On Linux the **pmem2_config_set_fd**() function validates and stores a file descriptor in pmem2_config.

On Windows the **pmem2_config_set_fd**() function converts a file descriptor to a file handle (using **_get_osfhandle**()), and passes it to **pmem2_config_set_handle**().
By default **_get_osfhandle**() calls abort() in case of invalid file descriptor, but this behavior can be suppressed by **_set_abort_behavior**() and **SetErrorMode**() functions.
Please check MSDN documentation for more information about Windows CRT error handling.

*fd* must be opened with *O_RDONLY* or *O_RDWR* mode, but on Windows it is not validated.

If *fd* is negative, then file descriptor (or handle on Windows) in *cfg is set to default, uninitialized value.

The **pmem2_config_set_handle**() function validates and stores a file handle in pmem2_config.
If *handle* is INVALID_HANDLE_VALUE, file descriptor (or handle on Windows) in *cfg is set to default, uninitialized value.

# RETURN VALUE #

**pmem2_config_set_fd**() and **pmem2_config_set_handle**() functions return 0 on success or one of the error values listed in the next section.

# ERRORS #
The **pmem2_config_set_fd**() function can return the following errors:

 * **PMEM2_E_INVALID_FILE_HANDLE** - *fd* is not an open file descriptor. On Windows the function can **abort**() on this failure based on CRT's abort() behavior.

 * **PMEM2_E_INVALID_FILE_HANDLE** - *fd* is opened in O_WRONLY mode.

On Linux:

 * **PMEM2_E_INVALID_FILE_TYPE** - *fd* points to a directory, block device, pipe, or socket.

 * **PMEM2_E_INVALID_FILE_TYPE** - *fd* points to a character device other than Device DAX.

On Windows:

 * **PMEM2_E_INVALID_FILE_TYPE** - *handle* points to a resource that is not a regular file.

On Windows **pmem2_config_set_fd**() can return all errors from the underlying **pmem2_config_set_handle**() function.

The **pmem2_config_set_handle**() can return the following errors:

 * **PMEM2_E_INVALID_FILE_HANDLE** - *handle* points to a resource that is not a file.

 * **PMEM2_E_INVALID_FILE_TYPE** - *handle* points to a directory.

# SEE ALSO #
**errno**(3), **pmem2_map**(3), **libpmem2**(7)
and **<http://pmem.io>**

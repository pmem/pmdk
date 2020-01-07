---
layout: manual
Content-Style: 'text/css'
title: _MP(PMEM2_CONFIG_GET_FILE_SIZE, 3)
collection: libpmem2
header: PMDK
date: pmem2 API version 1.0
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2019, Intel Corporation)

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

* **PMEM2_E_FILE_HANDLE_NOT_SET** - config doesn't contain the file handle
(see **pmem2_config_set_fd**(3), **pmem2_config_set_handle**(3)).

* **PMEM2_E_INVALID_FILE_HANDLE** - config contains an invalid file handle.

On Windows:

* **PMEM2_E_INVALID_FILE_TYPE** - handle points to a resource that is not
a regular file.

On Linux:

* **PMEM2_E_INVALID_FILE_TYPE** - file descriptor points to a directory,
block device, pipe, or socket.

* **PMEM2_E_INVALID_FILE_TYPE** - file descriptor points to a character
device other than Device DAX.

* **PMEM2_E_INVALID_SIZE_FORMAT** - kernel query for Device DAX size
returned data in invalid format.

* -**errno** set by failing **fstat**(2), while trying to validate the file
descriptor.

* -**errno** set by failing **realpath**(3), while trying to determine whether
fd points to a Device DAX.

* -**errno** set by failing **open**(2), while trying to determine Device DAX's
size.

* -**errno** set by failing **read**(2), while trying to determine Device DAX's
size.

* -**errno** set by failing **strtoull**(3), while trying to determine
Device DAX's size.

On FreeBSD:

* **PMEM2_E_INVALID_FILE_TYPE** - file descriptor points to a directory,
block device, pipe, socket, or character device.

* -**errno** set by failing **fstat**(2), while trying to validate the file
descriptor.

# SEE ALSO #

**errno**(3),  **fstat**(2), **realpath**(3), **open**(2), **read**(2),
**strtoull**(3), **pmem2_config_new**(3), **pmem2_config_set_handle**(3),
**pmem2_config_set_fd**(3), **libpmem2**(7) and **<http://pmem.io>**

---
draft: false
slider_enable: true
description: ""
disclaimer: "The contents of this web site and the associated <a href=\"https://github.com/pmem\">GitHub repositories</a> are BSD-licensed open source."
aliases: ["pmem2_source_size.3.html"]
title: "libpmem2 | PMDK"
header: "pmem2 API version 1.0"
---

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2019-2023, Intel Corporation)

[comment]: <> (pmem2_source_size.3 -- man page for pmem2_source_size)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[ERRORS](#errors)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmem2_source_size**() - returns the size of the data source

# SYNOPSIS #

```c
#include <libpmem2.h>

struct pmem2_source;
int pmem2_source_size(const struct pmem2_source *source, size_t *size);
```

# DESCRIPTION #

The **pmem2_source_size**() function retrieves the size of the file
in bytes pointed by file descriptor stored in the *source* and puts
it in *\*size*.

This function is a portable replacement for OS-specific APIs.
On Linux, it hides the quirkiness of Device DAX size detection.

# RETURN VALUE #

The **pmem2_source_size**() function returns 0 on success.
If the function fails, the *\*size* variable is left unmodified
and a negative error code is returned.

# ERRORS #

The **pmem2_source_size**() can fail with the following errors:

On all systems:

* **PMEM2_E_INVALID_FILE_HANDLE** - source contains an invalid file handle.

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
**strtoull**(3), **pmem2_config_new**(3), **libpmem2**(7)
and **<https://pmem.io>**

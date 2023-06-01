---
draft: false
slider_enable: true
description: ""
disclaimer: "The contents of this web site and the associated <a href=\"https://github.com/pmem\">GitHub repositories</a> are BSD-licensed open source."
aliases: ["pmem2_source_from_fd.3.html"]
title: "libpmem2 | PMDK"
header: "pmem2 API version 1.0"
---

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2019-2023, Intel Corporation)

[comment]: <> (pmem2_source_from_fd.3 -- man page for pmem2_source_from_fd

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[ERRORS](#errors)<br />
[CAVEATS](#caveats)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmem2_source_from_fd**(), **pmem2_source_delete**() - creates or deletes an instance of persistent memory
data source

# SYNOPSIS #

```c
#include <libpmem2.h>

int pmem2_source_from_fd(struct pmem2_source *src, int fd);
int pmem2_source_delete(struct pmem2_source **src);
```

# DESCRIPTION #

The **pmem2_source_from_fd**() function validates the file descriptor
and instantiates a new *struct pmem2_source** object describing the data source.

*fd* must be opened with *O_RDONLY* or *O_RDWR* mode.

If *fd* is invalid, then the function fails.

The **pmem2_source_delete**() function frees *\*src* returned by **pmem2_source_from_fd**()
and sets *\*src* to NULL. If *\*src* is NULL, no operation is performed.

# RETURN VALUE #

The **pmem2_source_from_fd**() function return 0 on success
or a negative error code on failure.

The **pmem2_source_delete**() function always returns 0.

# ERRORS #

The **pmem2_source_from_fd**() function can fail with the following errors:

 * **PMEM2_E_INVALID_FILE_HANDLE** - *fd* is not an open and valid file descriptor.

 * **PMEM2_E_INVALID_FILE_HANDLE** - *fd* is opened in O_WRONLY mode.

 * **PMEM2_E_INVALID_FILE_TYPE** - *fd* points to a directory, block device, pipe, or socket.

 * **PMEM2_E_INVALID_FILE_TYPE** - *fd* points to a character device other than Device DAX.

The **pmem2_source_from_fd**() function can also return **-ENOMEM**
in case of insufficient memory to allocate an instance of *struct pmem2_source*.

# SEE ALSO #

**errno**(3), **pmem2_map_new**(3), **libpmem2**(7)
and **<https://pmem.io>**

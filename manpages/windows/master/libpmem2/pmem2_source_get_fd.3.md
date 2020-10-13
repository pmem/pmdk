---
layout: manual
Content-Style: 'text/css'
title: PMEM2_SOURCE_GET_FD
collection: libpmem2
header: PMDK
date: pmem2 API version 1.0
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2020, Intel Corporation)

[comment]: <> (pmem2_source_get_fd.3 -- man page for pmem2_source_get_fd

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[ERRORS](#errors)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmem2_source_get_fd**() - reads file descriptor of the data source

# SYNOPSIS #

```c
#include <libpmem2.h>

int pmem2_source_get_fd(const struct pmem2_source *src, int *fd);
```

# DESCRIPTION #

The **pmem2_source_get_fd**() function reads the file descriptor of
*struct pmem2_source** object describing the data source and returns it
by *fd* parameter.

This function is Linux only, on Windows use **pmem2_source_get_handle**(3).

# RETURN VALUE #

The **pmem2_source_get_fd**() function returns 0 on success
or a negative error code on failure.

# ERRORS #

The **pmem2_source_get_fd**() can fail with the following errors:

* **PMEM2_E_FILE_DESCRIPTOR_NOT_SET** - in case of an instance of
*struct pmem2_source* that does not come from source type that
support file descriptors, eg. anonymous data source.

# SEE ALSO #

**pmem2_source_get_handle**(3), **libpmem2**(7) and **<http://pmem.io>**

---
layout: manual
Content-Style: 'text/css'
title: _MP(PMEM2_SOURCE_GET_HANDLE, 3)
collection: libpmem2
header: PMDK
date: pmem2 API version 1.0
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2020, Intel Corporation)

[comment]: <> (pmem2_source_get_handsle.3 -- man page for pmem2_source_get_handle

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[ERRORS](#errors)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmem2_source_get_handle**() - reads file handler of the data source

# SYNOPSIS #

```c
#include <libpmem2.h>

int pmem2_source_get_handle(const struct pmem2_source *src, HANDLE *h);
```

# DESCRIPTION #

The **pmem2_source_get_handle**() function reads the file handler of
*struct pmem2_source** object describing the data source and returns
it by *h* parameter.

This function is Windows only, on Linux use **pmem2_source_get_fd**(3).
If the source was created using **pmem2_source_from_fd**(3) then
**pmem2_source_get_handle**() is also valid function to read handler, because
file descriptor is converted to file handle during source creation.

However, there are limitations to what you can do with a handle created from a file descriptor.
For details refer to **DESCRIPTION** section in the **pmem2_source_from_fd**(3) manpage.

# ERRORS #

The **pmem2_source_get_handle**() can fail with the following errors:

* **PMEM2_E_FILE_HANDLE_NOT_SET** - in case of an instance of
*struct pmem2_source* that does not come from source type that
support file handles, eg. anonymous data source.

# RETURN VALUE #

The **pmem2_source_get_handle**() returns a file handle of data source.

# SEE ALSO #

**pmem2_source_from_fd**(3), **pmem2_source_get_fd**(3), **libpmem2**(7) and **<http://pmem.io>**

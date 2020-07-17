---
layout: manual
Content-Style: 'text/css'
title: PMEM2_SOURCE_ALIGNMENT
collection: libpmem2
header: PMDK
date: pmem2 API version 1.0
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2019-2020, Intel Corporation)

[comment]: <> (pmem2_source_alignment.3 -- man page for pmem2_source_alignment)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmem2_source_alignment**() - returns data source alignment

# SYNOPSIS #

```c
#include <libpmem2.h>

struct pmem2_source;
int pmem2_source_alignment(const struct pmem2_source *source, size_t *alignment);
```

# DESCRIPTION #

The **pmem2_source_alignment**() function retrieves the alignment of offset and
length needed for **pmem2_map_new**(3) to succeed. The alignment is stored in
*\*alignment*.

# RETURN VALUE #

The **pmem2_source_alignment**() function returns 0 on success.
If the function fails, the *\*alignment* variable is left unmodified, and one of
the following errors is returned:

On all systems:

* **PMEM2_E_INVALID_ALIGNMENT_VALUE** - operating system returned unexpected
alignment value (eg. it is not a power of two).

on Linux:

* **PMEM2_E_INVALID_FILE_TYPE** - file descriptor points to a character
device other than Device DAX.

* **PMEM2_E_INVALID_ALIGNMENT_FORMAT** - kernel query for Device DAX alignment
returned data in invalid format.

* -**errno** set by failing **fstat**(2), while trying to validate the file
descriptor.

* -**errno** set by failing **realpath**(3), while trying to determine whether
fd points to a Device DAX.

* -**errno** set by failing **read**(2), while trying to determine Device DAX's
alignment.

* -**errno** set by failing **strtoull**(3), while trying to determine
Device DAX's alignment.

On FreeBSD:

* **PMEM2_E_INVALID_FILE_TYPE** - file descriptor points to a directory,
block device, pipe, socket, or character device.

* -**errno** set by failing **fstat**(2), while trying to validate the file
descriptor.

# SEE ALSO #

**errno**(3),  **fstat**(2), **realpath**(3), **read**(2), **strtoull**(3),
**pmem2_config_new**(3), **pmem2_source_from_handle**(3),
**pmem2_source_from_fd**(3), **libpmem2**(7) and **<http://pmem.io>**

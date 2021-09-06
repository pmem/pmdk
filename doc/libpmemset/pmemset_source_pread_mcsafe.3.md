---
layout: manual
Content-Style: 'text/css'
title: _MP(PMEMSET_SOURCE_PREAD_MCSAFE, 3)
collection: libpmemset
header: PMDK
date: pmemset API version 1.0
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2021, Intel Corporation)

[comment]: <> (pmemset_source_pread_mcsafe.3 -- man page for libpmemset machine safe read/write operations)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[ERRORS](#errors)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmemset_source_pread_mcsafe**(), **pmemset_source_pwrite_mcsafe**() - read source
contents or write to the source in a safe manner

# SYNOPSIS #

```c
#include <libpmemset.h>

struct pmemset_source;
int pmemset_source_pread_mcsafe(struct pmemset_source *src, void *buf,
		size_t size, size_t offset);
int pmemset_source_pwrite_mcsafe(struct pmemset_source *src, void *buf,
		size_t size, size_t offset);
```

# DESCRIPTION #

The **pmemset_source_pread_mcsafe**() function reads *size* bytes from the source *src*
starting at offset *offset* into the buffer *buf*.
The **pmemset_source_pwrite_mcsafe**() function writes *size* bytes from the buffer *buf*
to the source *src* starting at the offset *offset*.

Safe manner refers to the capability to handle errors resulting from bad blocks, preventing
them from terminating an application and returning those errors to the user. For details, see
corresponding **libpmem2**(7) functions **pmem2_source_pread_mcsafe**(3) and **pmem2_source_pwrite_mcsafe**(3).

Source *src* can be created with **pmemset_source_from_file**(3), **pmemset_xsource_from_file**(3),
**pmemset_source_from_pmem2**(3) and **pmemset_source_from_temporary**(3).

# RETURN VALUE #

The **pmemset_source_pread_mcsafe**() and **pmemset_source_pwrite_mcsafe**() functions
return 0 on success or a negative error code on failure.

# ERRORS #

The **pmemset_source_pread_mcsafe**() and **pmemset_source_pwrite_mcsafe**() can fail
with the following errors:

* **PMEMSET_E_IO_FAIL** - a physical I/O error occured during the read/write operation,
a possible bad block encountered.

* **PMEMSET_E_LENGTH_OUT_OF_RANGE** - read/write operation size *size* from
offset *offset* goes beyond the file length. Sources created with
**pmemset_source_from_temporary**(3) have initial size 0 and are prone to this error.

Those operations can also return all errors from the underlying **pmem2_source_pread_mcsafe**(3) and **pmem2_source_pwrite_mcsafe**(3) functions.

# SEE ALSO #

**pmem2_source_pread_mcsafe**(3), **pmem2_source_pwrite_mcsafe**(3),
**pmemset_source_from_file**(3), **pmemset_source_from_pmem2**(3),
**pmemset_source_from_temporary**(3), **pmemset_xsource_from_file**(3),
**libpmem2**(7) and **<https://pmem.io>**

---
layout: manual
Content-Style: 'text/css'
title: PMEM2_SOURCE_PREAD_MCSAFE
collection: libpmem2
header: PMDK
date: pmem2 API version 1.0
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2021, Intel Corporation)

[comment]: <> (pmem2_source_pread_mcsafe.3 -- man page for libpmem2 machine safe read/write operations)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[ERRORS](#errors)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmem2_source_pread_mcsafe**(), **pmem2_source_pwrite_mcsafe**() - read source
contents or write to the source in a safe manner

# SYNOPSIS #

```c
#include <libpmem2.h>

struct pmem2_source;
int pmem2_source_pread_mcsafe(struct pmem2_source *src, void *buf, size_t size,
		size_t offset);
int pmem2_source_pwrite_mcsafe(struct pmem2_source *src, void *buf, size_t size,
		size_t offset);
```

# DESCRIPTION #

The **pmem2_source_pread_mcsafe**() function reads *size* bytes from the source *src*
starting at offset *offset* into the buffer *buf*.
The **pmem2_source_pwrite_mcsafe**() function writes *size* bytes from the buffer *buf*
to the source *src* starting at the offset *offset*.

Above functions are capable of detecting bad blocks and handling the *SIGBUS* signal thrown
when accessing a bad block. When a bad block is encountered, **pmem2_source_pread_mcsafe**()
and **pmem2_source_pwrite_mcsafe**() functions return corresponding error. A signal handler
for *SIGBUS* signal is registered using **sigaction**(2) for the running time of those operations.
This capability is limited to POSIX systems.

For bad block detection and clearing, see **pmem2_badblock_context_new**(3),
**pmem2_badblock_next**(3) and **pmem2_badblock_clear**(3).

# RETURN VALUE #

The **pmem2_source_pread_mcsafe**() and **pmem2_source_pwrite_mcsafe**() functions
return 0 on success or a negative error code on failure.

# ERRORS #

The **pmem2_source_pread_mcsafe**() and **pmem2_source_pwrite_mcsafe**() can fail
with the following errors:

* **PMEM2_E_IO_FAIL** - a physical I/O error occured during the read/write operation,
a possible bad block encountered.

* **PMEM2_E_LENGTH_OUT_OF_RANGE** - read/write operation size *size* from
offset *offset* goes beyond the file length.

* **PMEM2_E_SOURCE_TYPE_NOT_SUPPORTED** - read/write operation doesn't support
provided source, only sources created with **pmem2_source_from_fd**(3) and
**pmem2_source_from_handle**(3) are supported.

Those operations can also return all errors from the underlying **pread**(2),
**pwrite**(2), **sigaction**(2) functions on POSIX systems and **ReadFile**(),
**WriteFile**() functions on Windows.

# SEE ALSO #

**pread**(2), **pwrite**(2), **ReadFile**(), **WriteFile**(),
**pmem2_badblock_clear**(3), **pmem2_badblock_context_new**(3),
**pmem2_badblock_next**(3), **pmem2_source_from_fd**(3),
**pmem2_source_from_handle**(3),
**libpmem2**(7) and **<https://pmem.io>**

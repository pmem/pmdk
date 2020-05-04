---
layout: manual
Content-Style: 'text/css'
title: _MP(pmem2_badblock_context_new, 3)
collection: libpmem2
header: PMDK
date: pmem2 API version 1.0
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2020, Intel Corporation)

[comment]: <> (pmem2_badblock_context_new.3 -- man page for pmem2_badblock_context_new and pmem2_badblock_context_delete)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmem2_badblock_context_new**(), **pmem2_badblock_context_delete**() - allocate and free
a context for **pmem2_badblock_next**() and **pmem2_badblock_clear**() operations

# SYNOPSIS #

```c
#include <libpmem2.h>

struct pmem2_source;
struct pmem2_badblock_context;

int pmem2_badblock_context_new(
		const struct pmem2_source *src,
		struct pmem2_badblock_context **bbctx);

void pmem2_badblock_context_delete(
		struct pmem2_badblock_context **bbctx);
```

# DESCRIPTION #

The **pmem2_badblock_context_new**() function instantiates a new (opaque) bad block context structure, *pmem2_badblock_context*, which is used to read and clear bad blocks (by **pmem2_badblock_next**() and **pmem2_badblock_clear**()) and returns it through the pointer in *\*bbctx*.

New bad block context structure is initialized with values read from the source given as the first argument (*src*).

The **pmem2_badblock_context_delete**() function frees *\*bbctx* returned by **pmem2_badblock_context_new**() and sets *\*bbctx* to NULL. If *\*bbctx* is NULL, no operation is performed.

# RETURN VALUE #

The **pmem2_badblock_context_new**() function returns 0 on success or a negative error code on failure.
**pmem2_badblock_context_new**() does set *\*bbctx* to NULL on failure.

**pmem2_badblock_context_delete**() does not return any value.

Please see **libpmem2**(7) for detailed description of libpmem2 error codes.

# ERRORS #
**pmem2_badblock_context_new**() can fail with the following error:

- **-ENOMEM** - out of memory

* **-errno** - set by failing **ndctl_new**, while trying to create a new ndctl context.

* **-errno** - set by failing **fstat**(2), while trying to validate the file descriptor of *src*.

* **-errno** - set by failing **open**(2), while trying to open the FSDAX device matching with the *src*.

* **-errno** - set by failing **read**(2), while trying to read from the FSDAX device matching with the *src*.

* **-errno** - set by failing **snprintf**, while trying to validate a device path.

* **-errno** - set by failing **ndctl_region_get_resource**, while reading an offset of the region of the given *src*.

* **-errno** - set by failing **fiemap ioctl(2)**, while reading file extents of the given *src*.

* **PMEM2_E_INVALID_FILE_TYPE** - *src* is not a regular file nor a character device.

* **PMEM2_E_DAX_REGION_NOT_FOUND** - cannot find a DAX region for the given *src*.

* **PMEM2_E_CANNOT_READ_BOUNDS** - cannot read offset or size of the namespace of the given *src*.

# SEE ALSO #

**pmem2_badblock_next**(3), **pmem2_badblock_clear**(3),
**libpmem2**(7) and **<http://pmem.io>**

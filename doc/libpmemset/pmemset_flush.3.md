---
draft: false
slider_enable: true
description: ""
disclaimer: "The contents of this web site and the associated <a href=\"https://github.com/pmem\">GitHub repositories</a> are BSD-licensed open source."
aliases: ["pmemset_flush.3.html"]
title: "libpmemset | PMDK"
header: "pmemset API version 1.0"
---

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2021, Intel Corporation)

[comment]: <> (pmemset_flush.3 -- man page for libpmemset pmemset_flush function)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmemset_flush**() - flushes data from the range

# SYNOPSIS #

```c
#include <libpmemset.h>

int pmemset_flush(struct pmemset *set, const void *ptr, size_t size);
```

# DESCRIPTION #

The **pmemset_flush**() function efficiently flushes data from *set*
in the range [ptr, ptr + size]. The **pmemset_flush**() function automatically decides what is the
most appropriate mechanism for flushing stores directly to underlying storage.
Based on *granularity* value of the *set* and available platform, **pmemset_flush**() will use
system API calls such as msync(), fsync() on Linux, or FlushFileBuffers(), FlushViewOfFile() on Windows to flush data.
Depending on the architecture and *power-fail protected domain*, **pmemset_flush**() function can also call
machine instructions for flushing cache lines in more reliably way (e.g., CLWB, CLFLUSHOPT, CLFLUSH for Intel x86_64 architecture).

>NOTE: For more detailed information about *granularity* and possible flushing operation please see *GRANULARITY*
section in the **libpmem2**(7) man page.

>NOTE: In the underlying implementation **pmemset_flush**() uses *pmem2_flush_fn* returned by
**pmemset_get_flush_fn**(3), so all flush principles are identical for **pmemset_flush**()
function, and you can find them in the **pmem2_get_flush_fn(3)** man page.

# RETURN VALUE #

The **pmemset_flush**() function always returns 0.

# SEE ALSO #

**pmem2_get_flush_fn**(3), **libpmemset**(7), **libpmem2**(7),
and **<https://pmem.io>**

---
layout: manual
Content-Style: 'text/css'
title: PMEMSET_DEEP_FLUSH
collection: libpmemset
header: PMDK
date: pmemset API version 1.0
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2021, Intel Corporation)

[comment]: <> (pmemset_deep_flush.3 -- man page for libpmemset pmemset_deep_flush function)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmemset_deep_flush**() - performs deep flush on the range

# SYNOPSIS #

```c
#include <libpmemset.h>

int pmemset_deep_flush(struct pmemset *set, const void *ptr, size_t size);
```

# DESCRIPTION #

The **pmemset_deep_flush**() function forces any changes in the range [ptr, ptr+size) from the *set*
to be stored durably in the most reliable persistence domain available to software.
In particular, on supported platforms, this enables the code not to rely on automatic cache or
WPQ (write pending queue) flush on power failure (ADR/eADR).

Since this operation is usually much more expensive than regular persist, it should be used sparingly.
Typically, the application should only ever use this function as a precaution against hardware failures,
e.g., in code that detects silent data corruption caused by unsafe shutdown.

# RETURN VALUE #

The **pmemset_deep_flush**() function returns 0 on success
or a negative error code on failure.

# ERRORS #

The **pmemset_deep_flush**() can fail with the following errors:

* **PMEMSET_E_DEEP_FLUSH_FAIL** - the underlying device region id cannot be detected
 or cannot perform msync on a regular DAX volume.

# SEE ALSO #

**pmem2_get_persist_fn**(3), **libpmemset**(7), **libpmem2**(7),
and **<https://pmem.io>**

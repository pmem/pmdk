---
draft: false
slider_enable: true
description: ""
disclaimer: "The contents of this web site and the associated <a href=\"https://github.com/pmem\">GitHub repositories</a> are BSD-licensed open source."
aliases: ["pmem2_deep_flush.3.html"]
title: "libpmem2 | PMDK"
header: "pmem2 API version 1.0"
---

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2020, Intel Corporation)

[comment]: <> (pmem2_deep_flush.3 -- man page for pmem2_deep_flush)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[ERRORS](#errors)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmem2_deep_flush**() - highly reliable persistent memory synchronization

# SYNOPSIS #

```c
#include <libpmem2.h>

int pmem2_deep_flush(struct pmem2_map *map, void *ptr, size_t size)
```

# DESCRIPTION #

The **pmem2_deep_flush**() function forces any changes in the range \[*ptr*, *ptr*+*len*)
from the *map* to be stored durably in the most reliable persistence domain
available to software. In particular, on supported platforms, this enables
the code not to rely on automatic cache or WPQ (write pending queue) flush on power failure (ADR/eADR).

Since this operation is usually much more expensive than regular persist, it
should be used sparingly. Typically, the application should only ever use this
function as a precaution against hardware failures, e.g., in code that detects
silent data corruption caused by unsafe shutdown (see more in **libpmem2_unsafe_shutdown**(7)).

# RETURN VALUE #

The **pmem2_deep_flush**() function returns 0 on success
or an error code on failure.

# ERRORS #

The **pmem2_deep_flush**() can fail with the following errors:

* **PMEM2_E_DEEP_FLUSH_RANGE** - the provided flush range is not a
subset of the map's address space.

* **PMEM2_E_DAX_REGION_NOT_FOUND** - the underlying device region id cannot be
detected.

* -**errno** set by failing **msync**(2), while trying to perform
a deep flush on a regular DAX volume.

# SEE ALSO #

**msync**(2), **pmem2_get_drain_fn**(3), **pmem2_get_persist_fn**(3)
**pmem2_map**(3), **libpmem2**(7) and **<http://pmem.io>**

---
layout: manual
Content-Style: 'text/css'
title: _MP(PMEMSET_PERSIST, 3)
collection: libpmemset
header: PMDK
date: pmemset API version 1.0
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2021, Intel Corporation)

[comment]: <> (pmemset_persist.3 -- man page for libpmemset pmemset_persist function)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmemset_persist**() - persist data from the range

# SYNOPSIS #

```c
#include <libpmemset.h>

int pmemset_persist(struct pmemset *set, const void *ptr, size_t size);
```

# DESCRIPTION #

The **pmemset_persist**() function efficiently persisting data from *set*
in the range [ptr, ptr + size].

>NOTE: In the underlying implementation **pmemset_persist**() uses *pmem2_persist_fn* returned by
**pmemset_get_persist_fn**(3), so all flush principles are identical for **pmemset_persist**()
function, and you can find them in the **pmem2_get_persist_fn(3)** man page.

# RETURN VALUE #

The **pmemset_persist**() function always returns 0.

# SEE ALSO #

**pmem2_get_persist_fn**(3), **libpmemset**(7), **libpmem2**(7),
and **<https://pmem.io>**

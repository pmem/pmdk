---
layout: manual
Content-Style: 'text/css'
title: PMEMSET_DRAIN
collection: libpmemset
header: PMDK
date: pmemset API version 1.0
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2021, Intel Corporation)

[comment]: <> (pmemset_drain.3 -- man page for libpmemset pmemset_drain function)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmemset_drain**() - drain flushes

# SYNOPSIS #

```c
#include <libpmemset.h>

int pmemset_drain(struct pmemset *set);
```

# DESCRIPTION #

The **pmemset_drain**() function efficiently draining flushes in the range owned by *set*.
Each program that flushes discontiguous ranges using **pmemset_flush**(3) should follow up them by calling **pmemset_drain**()
to wait for drain hw buffers. Calling **pmemset_drain**() also guarantees proper ordering of the flushed stores.

>NOTE: For more detailed information about *granularity* and underlying drain operation please see *GRANULARITY*
section in the **libpmem2**(7) man page.

>NOTE: In the underlying implementation **pmemset_drain**() uses *pmem2_drain_fn* returned by
**pmemset_get_drain_fn**(3), so all drain principles are identical for **pmemset_drain**()
function, and you can find them in the **pmem2_get_drain_fn(3)** man page.

# RETURN VALUE #

The **pmemset_drain**() function always returns 0.

# SEE ALSO #

**pmem2_get_drain_fn**(3), **pmemset_flush**(3), **libpmemset**(7), **libpmem2**(7),
and **<https://pmem.io>**

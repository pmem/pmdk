---
layout: manual
Content-Style: 'text/css'
title: _MP(PMEMSET_GET_STORE_GRANULARITY, 3)
collection: libpmemset
header: PMDK
date: pmemset API version 1.0
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2020, Intel Corporation)

[comment]: <> (pmemset_get_store_granularity.3 -- man page for pmemset_get_store_granularity)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmemset_get_store_granularity**() - reads effective mappings granularity for pmemset

# SYNOPSIS #

```c
#include <libpmemset.h>

enum pmem2_granularity pmemset_get_store_granularity(struct pmemset *set);
```

# DESCRIPTION #

The **pmemset_get_store_granularity**() function reads granularity of the *set* object.

The *set* object has to contain at least one mapped part by **pmemset_part_map**(3) function,
otherwise reading granularity value is pointless and behavior of **pmemset_get_store_granularity**(3)
is undefined.

Concept of the granularity is described in **libpmem2**(7).
More information about granularity in the **libpmemset(7)** library
you can find in the **pmemset_config_set_required_store_granularity**(3) man page.

# RETURN VALUE

The **pmemset_get_store_granularity**() function returns
an effective granularity of the *set*.

# SEE ALSO #

**pmemset_config_set_required_store_graularity**(3),
**pmemset_part_map(3)**, libpmem2**(7), **libpmemset**(7) and **<http://pmem.io>**

---
layout: manual
Content-Style: 'text/css'
title: PMEM2_MAP_GET_STORE_GRANULARITY
collection: libpmem2
header: PMDK
date: pmem2 API version 1.0
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2020, Intel Corporation)

[comment]: <> (pmem2_map_get_store_granularity.3 -- man page for libpmem2 mapping)
[comment]: <> (operations)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmem2_map_get_store_granularity**() - reads effective mapping granularity

# SYNOPSIS #

```c
#include <libpmem2.h>

enum pmem2_granularity {
	PMEM2_GRANULARITY_BYTE,
	PMEM2_GRANULARITY_CACHE_LINE,
	PMEM2_GRANULARITY_PAGE,
};
enum pmem2_granularity pmem2_map_get_store_granularity(struct pmem2_map *map);
```

# DESCRIPTION #

The **pmem2_map_get_store_granularity**() function reads granularity of the created
mapping. The *map* parameter points to the structure describing mapping created
using the **pmem2_map_new**(3) function. Concept of the granularity is described in
**libpmem2**(7).

# RETURN VALUE #

The **pmem2_map_get_store_granularity**() function returns
a granularity of the mapped area.

# SEE ALSO #

**pmem2_map_new**(3), **libpmem2**(7) and **<https://pmem.io>**

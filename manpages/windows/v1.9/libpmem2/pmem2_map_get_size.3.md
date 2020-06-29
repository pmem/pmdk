---
layout: manual
Content-Style: 'text/css'
title: PMEM2_MAP_GET_SIZE
collection: libpmem2
header: PMDK
date: pmem2 API version 1.0
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2019, Intel Corporation)

[comment]: <> (pmem2_map_get_size.3 -- man page for libpmem2 mapping operations)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmem2_map_get_size**() - reads mapping size

# SYNOPSIS #

```c
#include <libpmem2.h>

size_t pmem2_map_get_size(struct pmem2_map *map);
```

# DESCRIPTION #

The **pmem2_map_get_size**() function reads size of the created mapping.
The *map* parameter points to the structure describing mapping created using
the **pmem2_map**(3) function.

# RETURN VALUE #

The **pmem2_map_get_size**() function returns a size of the mapped area.

# SEE ALSO #

**pmem2_map**(3), **libpmem2**(7) and **<https://pmem.io>**

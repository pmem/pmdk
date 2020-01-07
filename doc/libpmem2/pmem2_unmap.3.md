---
layout: manual
Content-Style: 'text/css'
title: _MP(PMEM2_UNMAP, 3)
collection: libpmem2
header: PMDK
date: pmem2 API version 1.0
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause
[comment]: <> (Copyright 2019-2020, Intel Corporation)

[comment]: <> (pmem2_unmap.3 -- man page for libpmem2 pmem2_unmap operation)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmem2_unmap**() - deletes a mapping

# SYNOPSIS #

```c
#include <libpmem2.h>

int pmem2_unmap(struct pmem2_map **map_ptr);
```

# DESCRIPTION #

The **pmem2_unmap**() function deletes the mapping described by the
*struct pmem2_map* object.

If **pmem2_unmap**() succeeds, deleting the mapping, it releases the
*struct pmem2_map* object describing it and writes a NULL value to *map_ptr*.
If the function fails, the *map_ptr* variable and the map object itself are left
unmodified and appropriate error value will be returned. For a list of possible
return values please see [RETURN VALUE](#return-value).

# RETURN VALUE #

When **pmem2_unmap**() succeeds it returns 0. Otherwise, it returns
one of the following error values:

* **PMEM2_E_MAPPING_NOT_FOUND** - mapping was not found (it was already
unmapped or pmem2_map state was corrupted)

On systems other than Windows it can also return **-EINVAL** from the underlying
**munmap**(2) function.

# SEE ALSO #

**pmem2_map(3)**, **libpmem2**(7) and **<http://pmem.io>**

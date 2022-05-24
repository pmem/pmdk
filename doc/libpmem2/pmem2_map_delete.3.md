---
draft: false
slider_enable: true
description: ""
disclaimer: "The contents of this web site and the associated <a href=\"https://github.com/pmem\">GitHub repositories</a> are BSD-licensed open source."
aliases: ["pmem2_map_delete.3.html"]
title: "libpmem2 | PMDK"
header: "pmem2 API version 1.0"
---

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause
[comment]: <> (Copyright 2019-2020, Intel Corporation)

[comment]: <> (pmem2_map_delete.3 -- man page for libpmem2 pmem2_map_delete operation)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[ERRORS](#errors)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmem2_map_delete**() - deletes a mapping

# SYNOPSIS #

```c
#include <libpmem2.h>

int pmem2_map_delete(struct pmem2_map **map_ptr);
```

# DESCRIPTION #

The **pmem2_map_delete**() function deletes the mapping described by the
*struct pmem2_map* object.

If **pmem2_map_delete**() succeeds in deleting the mapping, it releases the
*struct pmem2_map* object describing it and writes a NULL value to *map_ptr*.
If the function fails, the *map_ptr* variable and the map object itself are left
unmodified and appropriate error value is returned. For a list of possible
return values please see [RETURN VALUE](#return-value).

The **pmem2_map_delete**() function will not unmap mapping provided by the user
by **pmem2_map_from_existing**() function. In such case it will only free
*struct pmem2_map* object.

# RETURN VALUE #

The **pmem2_map_delete**() function returns 0 on success
or a negative error code on failure.

# ERRORS #

The **pmem2_map_delete**() can fail with the following errors:

* **PMEM2_E_MAPPING_NOT_FOUND** - mapping was not found (it was already
unmapped or pmem2_map state was corrupted)

On systems other than Windows it can also return **-EINVAL** from the underlying
**munmap**(2) function.

# SEE ALSO #

**pmem2_map_new(3)**, **libpmem2**(7) and **<https://pmem.io>**

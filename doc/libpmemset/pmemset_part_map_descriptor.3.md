---
layout: manual
Content-Style: 'text/css'
title: _MP(PMEMSET_PART_MAP_DESCRIPTOR, 3)
collection: libpmemset
header: PMDK
date: pmemset API version 1.0
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2020, Intel Corporation)

[comment]: <> (pmemset_part_map_descriptor.3 -- man page for libpmemset pmemset_part_map_descriptor operation)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmemset_part_map_descriptor**() - reads the address and size of part mapping

# SYNOPSIS #

```c
#include <libpmemset.h>

struct pmemset_part_map;
struct pmemset_part_descriptor pmemset_part_map_descriptor(struct pmemset_part_map *pmap);
```

# DESCRIPTION #

The **pmemset_part_map_descriptor**() function reads address and size of the part mapping
via  *pmap* parameter pointing to the structure describing part mapping. It can be obtained using
the **pmemset_part_map_first**(3) or the **pmemset_part_map_next**(3) function.

# RETURN VALUE #

The **pmemset_part_map_descriptor**() returns a *pmemset_part_descriptor* struct containing
descriptive information abot the part mapping.

# SEE ALSO #

**pmemset_part_map_first**(3), **pmemset_part_map_next**(3),
**libpmemset**(7) and **<http://pmem.io>**

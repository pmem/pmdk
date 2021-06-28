---
layout: manual
Content-Style: 'text/css'
title: PMEMSET_DESCRIPTOR_PART_MAP
collection: libpmemset
header: PMDK
date: pmemset API version 1.0
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2020, Intel Corporation)

[comment]: <> (pmemset_descriptor_part_map.3 -- man page for libpmemset pmemset_descriptor_part_map operation)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmemset_descriptor_part_map**() - reads the address and size of part mapping

# SYNOPSIS #

```c
#include <libpmemset.h>

struct pmemset_part_map;
struct pmemset_part_descriptor pmemset_descriptor_part_map(struct pmemset_part_map *pmap);
```

# DESCRIPTION #

The **pmemset_descriptor_part_map**() function reads address and size of the part mapping
via  *pmap* parameter pointing to the structure describing part mapping. It can be obtained using
the **pmemset_first_part_map**(3) or the **pmemset_next_part_map**(3) function.

# RETURN VALUE #

The **pmemset_descriptor_part_map**() returns a *pmemset_part_descriptor* struct containing
descriptive information abot the part mapping.

# SEE ALSO #

**pmemset_first_part_map**(3), **pmemset_next_part_map**(3),
**libpmemset**(7) and **<http://pmem.io>**

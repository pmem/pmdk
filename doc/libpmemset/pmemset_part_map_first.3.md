---
layout: manual
Content-Style: 'text/css'
title: _MP(PMEMSET_PART_MAP_FIRST, 3)
collection: libpmemset
header: PMDK
date: pmemset API version 1.0
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2020, Intel Corporation)

[comment]: <> (pmemset_part_map_first.3 -- man page for libpmemset pmemset_part_map_first operation)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmemset_part_map_first**() - reads first mapping from a set

# SYNOPSIS #

```c
#include <libpmemset.h>

struct pmemset;
struct pmemset_part_map;
void pmemset_part_map_first(struct pmemset *set, struct pmemset_part_map **pmap);
```

# DESCRIPTION #

The **pmemset_part_map_first**() function reads first mapping from the set.
The *set* parameter points to the structure describing a set created with **pmemset_new**(3) function.
Pointer to the first mapping object in the set is stored in the user-provided variable via the *pmap* pointer.

# RETURN VALUE #

The **pmemset_part_map_first**() does not return any value.
It sets *\*pmap* to NULL on failure.

# SEE ALSO #

**pmemset_new**(3),
**libpmemset**(7) and **<http://pmem.io>**

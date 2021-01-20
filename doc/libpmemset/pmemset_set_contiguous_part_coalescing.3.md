---
layout: manual
Content-Style: 'text/css'
title: _MP(PMEMSET_SET_CONTIGUOUS_PART_COALESCING, 3)
collection: libpmemset
header: PMDK
date: pmemset API version 1.0
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2020, Intel Corporation)

[comment]: <> (pmemset_set_contiguous_part_coalescing.3 -- man page for pmemset_set_contiguous_part_coalescing)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmemset_set_contiguous_part_coalescing**() - set part coalescing feature in the pmemset

# SYNOPSIS #

```c
#include <libpmemset.h>

struct pmemset;

int pmemset_set_contiguous_part_coalescing(struct pmemset *set, bool value);
```

# DESCRIPTION #

The **pmemset_set_contiguous_part_coalescing**() sets part coalescing feature flag in
pmemset to the provided value.

When part coalescing feature is turned on each part mapped, using **pmemset_part_map**(3) function,
to the pmemset with this feature is coalesced with the previous part and. Coalesced mapped parts
appear as single *struct pmemset_part_map* and can be retrieved by iterating over the pmemset
using **pmemset_first_part_map**(3) and **pmemset_next_part_map**(3), or simply by retrieving
part by its mapping address with **pmemset_part_map_by_address**(3) function.

# RETURN VALUE

The **pmemset_set_contiguous_part_coalescing**() function always returns 0.

# SEE ALSO #

**pmemset_part_map(3)**, **pmemset_first_part_map**(3),
**pmemset_next_part_map**(3),
**libpmemset**(7) and **<http://pmem.io>**

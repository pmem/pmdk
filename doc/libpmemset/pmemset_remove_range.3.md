---
draft: false
slider_enable: true
description: ""
disclaimer: "The contents of this web site and the associated <a href=\"https://github.com/pmem\">GitHub repositories</a> are BSD-licensed open source."
aliases: ["pmemset_remove_range.3.html"]
title: "libpmemset | PMDK"
header: "pmemset API version 1.0"
---

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2021, Intel Corporation)

[comment]: <> (pmemset_remove_range.3 -- man page for libpmemset pmemset_remove_range operation)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[ERRORS](#errors)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmemset_remove_range**() - removes mapped parts overlapping with the provided range

# SYNOPSIS #

```c
#include <libpmemset.h>

struct pmemset;
int pmemset_remove_range(struct pmemset *set, void *addr, size_t len);
```

# DESCRIPTION #

**pmemset_remove_range**() - removes and unmaps existing parts that were mapped to the *set*,
overlapping with virtual memory range described by the *addr* and *len* variables. The part mappings
residing at the provided range can't be referenced by any of the threads for this function to succeed.
References acquired using either **pmemset_first_part_map**(3), **pmemset_next_part_map**(3) or
**pmemset_part_map_by_address**(3) can be dropped with **pmemset_part_map_drop**(3) function.

This function will work even if the part mappings contain underlying coalesced parts. It means that
if the part map consists of two coalesced parts and the provided range intersects only with one of
those parts, then only the intersected part will be removed. For more information about coalesced part
mappings see **pmemset_set_contiguous_part_coalescing(3)**.

# RETURN VALUE #

The **pmemset_remove_range**() function returns 0 on success or a negative error code on failure.

# ERRORS #

The **pmemset_remove_range**() function can fail with the following errors:

* **PMEMSET_E_CANNOT_FIND_PART_MAP** - provided range doesn't overlap with any part mapping stored
in the pmemset

* **PMEMSET_E_PART_MAP_POSSIBLE_USE_AFTER_DROP** - part mapping residing at the provided range is
referenced by some thread

It can also return errors from the underlying **pmem2_map_delete**(3) function from the **libpmem2**(7) library.

# SEE ALSO #

**pmem2_map_delete**(3), **pmemset_first_part_map**(3),
**pmemset_next_part_map**(3), **pmemset_part_map_by_address**(3),
**pmemset_part_map_drop**(3), **pmemset_set_contiguous_part_coalescing(3)**,
**libpmem2**(7), **libpmemset**(7) and **<http://pmem.io>**

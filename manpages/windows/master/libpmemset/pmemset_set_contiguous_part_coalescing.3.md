---
layout: manual
Content-Style: 'text/css'
title: PMEMSET_SET_CONTIGUOUS_PART_COALESCING
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
[ERRORS](#errors)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmemset_set_contiguous_part_coalescing**() - set part coalescing feature in the pmemset

# SYNOPSIS #

```c
#include <libpmemset.h>

enum pmemset_coalescing;
struct pmemset;

int pmemset_set_contiguous_part_coalescing(struct pmemset *set,
		enum pmemset_coalescing value);
```

# DESCRIPTION #

The **pmemset_set_contiguous_part_coalescing**() sets part coalescing feature flag in
pmemset to the provided *value*. The possible values are *PMEMSET_COALESCING_NONE*,
*PMEMSET_COALESCING_OPPORTUNISTIC* and *PMEMSET_COALESCING_FULL*.

When part coalescing is enabled, the **pmemset_map**(3) function will try to coalesce each
new mapped part with the previously mapped part, that means it will try to map the part directly
after the previous mapping. The behavior of part mapping can be changed by setting one of possible
values in pmemset:

* *PMEMSET_COALESCING_NONE* - default behavior, no new mapped part will be coalesced, the position
of each mapped part in virtual address space is chosen arbitrarily by the operating system

* *PMEMSET_COALESCING_OPPORTUNISTIC* - each new mapped part will possibly be coalesced but if it's
not possible it will be handled like with *PMEMSET_COALESCING_NONE* value set

* *PMEMSET_COALESCING_FULL* - each new mapped part will be coalesced, if it's not possible
the mapping will fail

Mapping parts contiguously allows modifying the virtual address space of multiple parts with one
operation using for example **memset**(3). The success of the part coalescing depends on the operating
system and is not guaranteed. For more information see **pmemset_map**(3).

Coalesced parts appear as single *struct pmemset_part_map* and can be retrieved by iterating over
the pmemset using **pmemset_first_part_map**(3) and **pmemset_next_part_map**(3) or
simply by retrieving part by its mapping address with **pmemset_part_map_by_address**(3) function.

# RETURN VALUE

The **pmemset_set_contiguous_part_coalescing**() function returns 0 on success
or a negative error code on failure.

# ERRORS #

The **pmemset_set_contiguous_part_coalescing**() can fail with the following errors:

* **PMEMSET_E_INVALID_COALESCING_VALUE** - contiguous part coalescing value not one
of the possible values.

# SEE ALSO #

**pmemset_first_part_map**(3), **pmemset_map(3)**,
**pmemset_next_part_map**(3), **pmemset_part_map_by_address**(3),
**libpmemset**(7) and **<http://pmem.io>**

---
draft: false
slider_enable: true
description: ""
disclaimer: "The contents of this web site and the associated <a href=\"https://github.com/pmem\">GitHub repositories</a> are BSD-licensed open source."
aliases: ["pmemset_first_part_map.3.html"]
title: "libpmemset | PMDK"
header: "pmemset API version 1.0"
---

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2020, Intel Corporation)

[comment]: <> (pmemset_first_part_map.3 -- man page for libpmemset pmemset_first_part_map operation)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmemset_first_part_map**() - reads first mapping from a set
**pmemset_next_part_map**() - reads next mapping from a set

# SYNOPSIS #

```c
#include <libpmemset.h>

struct pmemset;
struct pmemset_part_map;
void pmemset_first_part_map(struct pmemset *set, struct pmemset_part_map **pmap);

void pmemset_next_part_map(struct pmemset *set, struct pmemset_part_map *cur,
		struct pmemset_part_map **next)
```

# DESCRIPTION #

The **pmemset_first_part_map**() function reads first mapping from the *set*.
The **pmemset_next_part_map**() function reads from the *set* a mapping  that succeeds the mapping
provided in the *cur* variable.
The *set* parameter points to the structure describing a set created with **pmemset_new**(3) function.
Pointer to the retrieved mapping object in the *set* is stored in the user-provided variable via the *pmap*
or *next* pointer depending on the used function.

# RETURN VALUE #

The **pmemset_first_part_map**() does not return any value.
*\*pmap* is set to NULL on failure.

The **pmemset_next_part_map**() does not return any value.
*\*next* is set to NULL on failure.

# SEE ALSO #

**pmemset_new**(3),
**libpmemset**(7) and **<http://pmem.io>**

---
draft: false
slider_enable: true
description: ""
disclaimer: "The contents of this web site and the associated <a href=\"https://github.com/pmem\">GitHub repositories</a> are BSD-licensed open source."
aliases: ["pmemset_part_map_drop.3.html"]
title: "libpmemset | PMDK"
header: "pmemset API version 1.0"
---

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2021, Intel Corporation)

[comment]: <> (pmemset_part_map_drop.3 -- man page for libpmemset pmemset_part_map_drop operation)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmemset_part_map_drop**() - drops the reference to the part mapping

# SYNOPSIS #

```c
#include <libpmemset.h>

struct pmemset_part_map;

void pmemset_part_map_drop(struct pmemset_part_map **pmap);
```

# DESCRIPTION #

The **pmemset_part_map_drop**() function drops the address reference to the part mapping.
Address of a pointer to the part mapping object is passed to the function via the *pmap* pointer.
Pointer to the part mapping object can be retrieved using either **pmemset_first_part_map**(3),
**pmemset_next_part_map**(3) or **pmemset_part_map_by_address**(3) functions.

# RETURN VALUE #

The **pmemset_part_map_drop**() does not return any value.
*\*pmap* is always set to NULL.

# SEE ALSO #

**pmemset_first_part_map**(3), **pmemset_next_part_map**(3),
**pmemset_part_map_by_address**(3),
**libpmemset**(7) and **<https://pmem.io>**

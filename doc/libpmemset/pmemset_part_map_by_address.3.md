---
draft: false
slider_enable: true
description: ""
disclaimer: "The contents of this web site and the associated <a href=\"https://github.com/pmem\">GitHub repositories</a> are BSD-licensed open source."
aliases: ["pmemset_part_map_by_address.3.html"]
title: "libpmemset | PMDK"
header: "pmemset API version 1.0"
---

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2021, Intel Corporation)

[comment]: <> (pmemset_part_map_by_address.3 -- man page for libpmemset pmemset_part_map_by_address function)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[ERRORS](#errors)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmemset_part_map_by_address**() - returns part map object

# SYNOPSIS #

```c
#include <libpmemset.h>

struct pmemset;
struct pmemset_part_map;
int pmemset_part_map_by_address(struct pmemset *set, struct pmemset_part_map **pmap,
		void *addr);
```

# DESCRIPTION #

The **pmemset_part_map_by_address**() - returns *part_map* object from the *set*.
The *part_map* has to contain address *addr*.

# RETURN VALUE #

The **pmemset_part_map_by_address**() function returns 0 on success
or a negative error code on failure.

# ERRORS #

The **pmemset_part_map_by_address**() can fail with the following errors:

* **PMEMSET_E_CANNOT_FIND_PART_MAP** - *set* does not contain part map at address *addr*.

# SEE ALSO #

**libpmemset**(7) and **<https://pmem.io>**

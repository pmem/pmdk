---
draft: false
slider_enable: true
description: ""
disclaimer: "The contents of this web site and the associated <a href=\"https://github.com/pmem\">GitHub repositories</a> are BSD-licensed open source."
aliases: ["pmemset_get_store_granularity.3.html"]
title: "libpmemset | PMDK"
header: "pmemset API version 1.0"
---

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2020-2021, Intel Corporation)

[comment]: <> (pmemset_get_store_granularity.3 -- man page for pmemset_get_store_granularity)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[ERRORS](#errors)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmemset_get_store_granularity**() - reads effective mappings granularity for pmemset

# SYNOPSIS #

```c
#include <libpmemset.h>

int pmemset_get_store_granularity(struct pmemset *set, enum pmem2_granularity *g);
```

# DESCRIPTION #

The **pmemset_get_store_granularity**() function reads effective granularity of the *set* object and puts it in the *\*g*;

The *set* object has to contain at least one mapped part using **pmemset_map**(3) function,
otherwise reading granularity value is pointless and function **pmemset_get_store_granularity**(3) will fail.

Concept of the granularity is described in **libpmem2**(7).

# RETURN VALUE

The **pmemset_get_store_granularity**() function returns 0 on success or a negative error code on failure.

# ERRORS #

**pmemset_get_store_granularity**() can fail with the following error:

- **PMEMSET_E_NO_PART_MAPPED** - cannot read effective granularity of the *set* because it does not contain any mapped *parts*.

# SEE ALSO #

**pmemset_config_set_required_store_graularity**(3),
**pmemset_map(3)**, libpmem2**(7), **libpmemset**(7) and **<http://pmem.io>**

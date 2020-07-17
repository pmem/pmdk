---
layout: manual
Content-Style: 'text/css'
title: PMEM2_CONFIG_SET_SHARING
collection: libpmem2
header: PMDK
date: pmem2 API version 1.0
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2020, Intel Corporation)

[comment]: <> (pmem2_config_set_sharing.3 -- man page for libpmem2 config API)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmem2_config_set_sharing**() - set sharing in the pmem2_config structure

# SYNOPSIS #

```c
#include <libpmem2.h>

struct pmem2_config;
enum pmem2_sharing_type {
	PMEM2_SHARED,
	PMEM2_PRIVATE,
};
int pmem2_config_set_sharing(struct pmem2_config *config, enum pmem2_sharing_type sharing);
```

# DESCRIPTION #

The **pmem2_config_set_sharing**() function configures the behavior and visibility
of writes to the mapping's pages. The possible values are listed below:

* **PMEM2_SHARED** - Writes are made directly to the underlying memory, making
them visible to other mappings of the same memory region. (default)

* **PMEM2_PRIVATE** - Writes do not affect the underlying memory and are
not visible to other mappings of the same memory region.

# RETURN VALUE #

The **pmem2_config_set_sharing**() function returns 0 on success. Otherwise, it
returns:

* **PMEM2_E_INVALID_SHARING_VALUE** - *sharing* value is invalid.

# SEE ALSO #

**libpmem2**(7), **pmem2_config_new**(3), **pmem2_map_new**(3), **sysconf**(3)
and **<http://pmem.io>**

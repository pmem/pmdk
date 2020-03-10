---
layout: manual
Content-Style: 'text/css'
title: _MP(PMEM2_CONFIG_SET_SHARING, 3)
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
int pmem2_config_set_sharing(struct pmem2_config *config, enum pmem2_sharing_type sharing);
```

# DESCRIPTION #

The **pmem2_config_set_sharing**() function allows deciding if changes applied
to the mapping will be visible to other processes and if they will be present
after remapping.

Possible sharing values are:

* **PMEM2_SHARED** - Changes made to the mapped memory are transferred to
the source of the memory, making them visible to other mappings from
the same source. (default)

* **PMEM2_PRIVATE** - Changes made to the mapped memory are private and won't
reach the source. So they are not visible to another mapping from the same source
and won't be visible after remapping.

# RETURN VALUE #

The **pmem2_config_set_sharing**() function returns 0 on success. Otherwise, it
returns:

* **PMEM2_E_INVALID_SHARING_VALUE** - *sharing* value is invalid.

# SEE ALSO #

**libpmem2**(7), **pmem2_config_new**(3), **pmem2_map**(3), **sysconf**(3)
and **<http://pmem.io>**

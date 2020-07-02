---
layout: manual
Content-Style: 'text/css'
title: _MP(PMEM2_CONFIG_CLEAR_ADDRESS, 3)
collection: libpmem2
header: PMDK
date: pmem2 API version 1.0
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2020, Intel Corporation)

[comment]: <> (pmem2_config_clear_address.3 -- man page for libpmem2 config API)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmem2_config_clear_address**() - reset addr and request_type to the default values
in the pmem2_config structure

# SYNOPSIS #

```c
#include <libpmem2.h>

struct pmem2_config;
void pmem2_config_clear_address(struct pmem2_config *cfg);
```

# DESCRIPTION #

The **pmem2_config_clear_address**() function resets *\*addr* and \**request_type* to the default values.
The function is used to revert changes set by **pmem2_config_set_address**(3).
If the *\*addr* is default, the starting mapping address will be chosen by the operating system, for
more information please see **pmem2_map_new**(3).

# RETURN VALUE #

**pmem2_config_clear_address**() does not return any value.

# SEE ALSO #

**libpmem2**(7), **pmem2_config_set_address**(3), **pmem2_map_new**(3), and **<http://pmem.io>**

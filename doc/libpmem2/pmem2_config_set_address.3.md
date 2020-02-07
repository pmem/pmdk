---
layout: manual
Content-Style: 'text/css'
title: _MP(PMEM2_CONFIG_SET_ADDRESS, 3)
collection: libpmem2
header: PMDK
date: pmem2 API version 1.0
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2019, Intel Corporation)

[comment]: <> (pmem2_config_set_address.3 -- man page for libpmem2 config API)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmem2_config_set_address**() - set address among with address request type in the
pmem2_config structure

# SYNOPSIS #

```c
#include <libpmem2.h>

struct pmem2_config;
int pmem2_config_set_address(struct pmem2_config *cfg, void *addr, int request_type);
```

# DESCRIPTION #

The **pmem2_config_set_address**() function configures the address among with address request
type which will be used for mapping. It takes one address request type only. *\*config* should
be already initialized, please see **pmem2_config_new**(3) for details. The *\*addr* cannot be
**NULL** and must be a multiple of the alignment required for the data source which will be used
for mapping alongside the config. To retrieve the alignment required for specific instance of
**pmem2_source** use **pmem2_source_alignment**(3).

Possible address request types are:

* **PMEM2_ADDRESS_FIXED_NOREPLACE** - try to place the mapping at exactly that address
which was set by user. When address is occupied, it never clobbers a preexisting mapped range
and **pmem2_map**(3) returns **PMEM2_E_MAPPING_EXISTS**.

# RETURN VALUE #

When **pmem2_config_set_address**() succeeds it returns 0. Otherwise, it returns one of
the following error values:

* **PMEM2_E_INVALID_ADDRESS_TYPE** - set address request type is invalid.

* **PMEM2_E_ADDRESS_UNINITIALIZED** - cannot use address request type **PMEM2_ADDRESS_FIXED_NOREPLACE**
when address is **NULL**.

# SEE ALSO #

**libpmem2**(7), **pmem2_config_new**(3), **pmem2_map**(3), **pmem2_source_alignment**(3),
**sysconf**(3) and **<http://pmem.io>**

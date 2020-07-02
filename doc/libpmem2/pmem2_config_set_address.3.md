---
layout: manual
Content-Style: 'text/css'
title: _MP(PMEM2_CONFIG_SET_ADDRESS, 3)
collection: libpmem2
header: PMDK
date: pmem2 API version 1.0
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2020, Intel Corporation)

[comment]: <> (pmem2_config_set_address.3 -- man page for libpmem2 config API)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmem2_config_set_address**() - set requested address in the pmem2_config structure

# SYNOPSIS #

```c
#include <libpmem2.h>

struct pmem2_config;
enum pmem2_address_request_type {
	PMEM2_ADDRESS_FIXED_REPLACE,
	PMEM2_ADDRESS_FIXED_NOREPLACE,
};
int pmem2_config_set_address(struct pmem2_config *cfg, void *addr,
		enum pmem2_address_request_type request_type);
```

# DESCRIPTION #

The **pmem2_config_set_address**() function sets the starting address *\*addr* which will be used
for memory mapping. If the *\*addr* is not specified in the config, the starting address
will be chosen by the operating system. The *\request_type* specifies how strictly the address
should be enforced. *\*config* should be already initialized, please see **pmem2_config_new**(3)
for details. The *\*addr* cannot be **NULL** and must be a multiple of the alignment required for the
data source which will be used for mapping alongside the config. To retrieve the alignment required
for specific instance of **pmem2_source** use **pmem2_source_alignment**(3). To reset *\*addr* and
*\request_type* to the default values, please use **pmem2_config_clear_address**(3).

Possible address request types are:

* **PMEM2_ADDRESS_FIXED_REPLACE** - not supported yet.

* **PMEM2_ADDRESS_FIXED_NOREPLACE** - *\*addr* cannot be **NULL**, kernel tries to place the mapping
at exactly the address which was set by user. When any part of <*\*addr*, *\*addr* + length> address
space is occupied, **pmem2_map_new**(3) fails with the **PMEM2_E_MAPPING_EXISTS** return code.

# RETURN VALUE #

When **pmem2_config_set_address**() succeeds it returns 0. Otherwise, it returns one of the following
error values:

* **PMEM2_E_INVALID_ADDRESS_REQUEST_TYPE** - set address request type is invalid.

* **PMEM2_E_ADDRESS_NULL** - cannot use address request type **PMEM2_ADDRESS_FIXED_NOREPLACE**
when address is **NULL**.

# SEE ALSO #

**libpmem2**(7), **pmem2_config_clear_address**(3), **pmem2_config_new**(3), **pmem2_map_new**(3),
**pmem2_source_alignment**(3), **sysconf**(3) and **<http://pmem.io>**

---
layout: manual
Content-Style: 'text/css'
title: PMEM2_CONFIG_SET_REQUIRED_STORE_GRANULARITY
collection: libpmem2
header: PMDK
date: pmem2 API version 1.0
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2019, Intel Corporation)

[comment]: <> (pmem2_config_set_required_store_granularity.3 -- man page for pmem2_config_set_required_store_granularity

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmem2_config_set_required_store_granularity**() - set a granularity
in pmem2_config structure.

# SYNOPSIS #

```c
#include <libpmem2.h>

enum pmem2_granularity {
	PMEM2_GRANULARITY_BYTE,
	PMEM2_GRANULARITY_CACHE_LINE,
	PMEM2_GRANULARITY_PAGE,
};
int pmem2_config_set_required_store_granularity(struct pmem2_config *cfg,
		enum pmem2_granularity g);
```

# DESCRIPTION #

The **pmem2_config_set_required_store_granularity**() sets a maximum permitted
granularity *g* requested by user in the *pmem2_config* structure.

Granularity must be one of the following values:

 * **PMEM2_GRANULARITY_BYTE**

 * **PMEM2_GRANULARITY_CACHE_LINE**

 * **PMEM2_GRANULARITY_PAGE**

A description of the granularity concept can be found in **libpmem2**(7) manpage.

# RETURN VALUE #

**pmem2_config_set_required_store_granularity**() function returns 0 on success.
Otherwise, it returns one of the following error values:

 * **PMEM2_E_INVALID_ARG** - granularity *g* is not a valid value.

# SEE ALSO #
**pmem2_config_new**(3), **libpmem2**(7)
and **<http://pmem.io>**

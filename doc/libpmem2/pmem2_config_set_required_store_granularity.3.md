---
draft: false
slider_enable: true
description: ""
disclaimer: "The contents of this web site and the associated <a href=\"https://github.com/pmem\">GitHub repositories</a> are BSD-licensed open source."
aliases: ["pmem2_config_set_required_store_granularity.3.html"]
title: "libpmem2 | PMDK"
header: "pmem2 API version 1.0"
---

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2020, Intel Corporation)

[comment]: <> (pmem2_config_set_required_store_granularity.3 -- man page for pmem2_config_set_required_store_granularity

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[ERRORS](#errors)<br />
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

The **pmem2_config_set_required_store_granularity**() function returns 0 on success
or a negative error code on failure.

# ERRORS #

The **pmem2_config_set_required_store_granularity**() can fail
with the following errors:

 * **PMEM2_E_GRANULARITY_NOT_SUPPORTED** - granularity *g* is not a valid value.

# SEE ALSO #
**pmem2_config_new**(3), **libpmem2**(7)
and **<https://pmem.io>**

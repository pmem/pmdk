---
draft: false
slider_enable: true
description: ""
disclaimer: "The contents of this web site and the associated <a href=\"https://github.com/pmem\">GitHub repositories</a> are BSD-licensed open source."
aliases: ["pmem2_map_from_existing.3.html"]
title: "libpmem2 | PMDK"
header: "pmem2 API version 1.0"
---

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2020-2021, Intel Corporation)

[comment]: <> (pmem2_map_from_existing.3 -- man page for libpmem2 pmem2_map_from_existing operation)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[ERRORS](#errors)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmem2_map_from_existing**() - creates a pmem2_map object from an existing mapping

# SYNOPSIS #

```c
#include <libpmem2.h>

int pmem2_map_from_existing(struct pmem2_map **map, const struct pmem2_source *src,
	void *addr, size_t len, enum pmem2_granularity gran);

```

# DESCRIPTION #

The **pmem2_map_from_existing**() returns a new *struct pmem2_map** for mapping
provided by the user. This function allows usage of **libpmem2**(7) API without **pmem2_map_new**(3) for mapping file.
Mapping is defined by *addr* and *len*. You have to specify underlying file as a *src*, and define granularity of this mapping.
See **pmem2_config_set_required_store_granularity**(3) and **libpmem2**(7) for more details.

For the *pmem2_map* object created by the **pmem2_map_from_existing**(3) function, the **pmem2_map_delete**(3) will only destroy the object,
but it won't unmap the mapping this object describes.

# RETURN VALUE #

The **pmem2_map_from_existing**() function returns 0 when it succeeds
or a negative error code on failure.

# ERRORS #

The **pmem2_map_from_existing**() can fail with the following errors:

**PMEM2_E_MAPPING_EXISTS** - when contiguous region (*addr*, *addr* + *len*)
is already registered by *libpmem2*

It can also return **-ENOMEM**  from the underlying **malloc**(2) function.

# SEE ALSO #

**malloc(2)**, **pmem2_map_delete**(3), **pmem2_map_new**(3),
**pmem2_source_from_fd**(3), **libpmem2**(7) and **<https://pmem.io>**

---
layout: manual
Content-Style: 'text/css'
title: _MP(PMEMSET_MAP_CONFIG_SET_SOURCE, 3)
collection: libpmemset
header: PMDK
date: pmemset API version 1.0
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2021, Intel Corporation)

[comment]: <> (pmemset_map_config_set_source.3 -- man page for pmemset_map_config_set_source)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[ERRORS](#errors)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmemset_map_config_set_source**() - set source of mapping.

# SYNOPSIS #

```c
#include <libpmemset.h>

int pmemset_config_set_source(struct pmemset_map_config *map_cfg,
	struct pmemset_source *src);
```

# DESCRIPTION #

The **pmemset_map_config_set_source**() sets source *src* for future mapping in the *map_cfg* structure.

New mapping is created using **pmemset_map**(3) function based on the source specified in the *\*src* pointer.

For the operation to succeed the *src* structure cannot be a *NULL* value and must be created from a valid data source.
See **pmemset_source_from_file**(3) and **pmemset_source_from_pmem2**(3) for possible sources.

# RETURN VALUE

The **pmemset_map_config_set_source**() function returns 0 on success or a negative error code on failure.

# ERRORS #

The **pmemset_map_config_set_source**() can fail with the following error:

* **PMEMSET_E_INVALID_SOURCE_TYPE** - source *src* is not a valid value.

# SEE ALSO #

**pmemset_map**(3), **pmemset_source_from_file**(3),
**pmemset_source_from_pmem2**(3), **libpmemset**(7) and **<http://pmem.io>**

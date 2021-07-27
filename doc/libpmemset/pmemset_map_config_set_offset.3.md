---
layout: manual
Content-Style: 'text/css'
title: _MP(PMEMSET_MAP_CONFIG_SET_OFFSET, 3)
collection: libpmemset
header: PMDK
date: pmemset API version 1.0
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2021, Intel Corporation)

[comment]: <> (pmemset_map_config_set_offset.3 -- man page for pmemset_map_config_set_offset)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[ERRORS](#errors)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmemset_map_config_set_offset**() - set offset of mapping in the map config.

# SYNOPSIS #

```c
#include <libpmemset.h>

int pmemset_config_set_offset(struct pmemset_map_config *map_cfg,
	size_t offset);
```

# DESCRIPTION #

The **pmemset_map_config_set_offset**() sets offset *offset* for future mapping in the *map_cfg* structure.

New mapping is created using **pmemset_map**(3) function and the offset specified in the *offset* value.

# RETURN VALUE

The **pmemset_config_set_offset**() function returns 0 on success
or a negative error code on failure.

# ERRORS #

The **pmemset_config_set_offset**() can fail with the following errors:

* **PMEMSET_E_OFFSET_OUT_OF_RANGE** - argument out of range, offset is greater than
**INT64_MAX**

# SEE ALSO #

**pmemset_map**(3), **libpmemset**(7) and **<http://pmem.io>**

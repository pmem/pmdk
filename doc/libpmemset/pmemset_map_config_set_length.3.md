---
layout: manual
Content-Style: 'text/css'
title: _MP(PMEMSET_MAP_CONFIG_SET_LENGTH, 3)
collection: libpmemset
header: PMDK
date: pmemset API version 1.0
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2021, Intel Corporation)

[comment]: <> (pmemset_map_config_set_length.3 -- man page for pmemset_map_config_set_length)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmemset_map_config_set_length**() - set length of mapping in the map config.

# SYNOPSIS #

```c
#include <libpmemset.h>

void pmemset_config_set_length(struct pmemset_map_config *map_cfg,
	size_t length);
```

# DESCRIPTION #

The **pmemset_map_config_set_length**() sets length *length* for future mapping in the *map_cfg* structure.

New mapping is created using **pmemset_map**(3) function and the length specified in the *length* value.

# RETURN VALUE

The **pmemset_map_config_set_length**() function returns no value.

# SEE ALSO #

**pmemset_map**(3), **libpmemset**(7) and **<http://pmem.io>**

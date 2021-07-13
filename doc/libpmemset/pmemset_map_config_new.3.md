---
layout: manual
Content-Style: 'text/css'
title: _MP(PMEMSET_MAP_CONFIG_NEW, 3)
collection: libpmemset
header: PMDK
date: pmemset API version 1.0
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2021, Intel Corporation)

[comment]: <> (pmemset_map_config_new.3 -- man page for libpmemset pmemset_map_config_new operation)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[ERRORS](#errors)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmemset_map_config_new**(), **pmemset_map_config_delete**() - create and delete structure
for a map config object

# SYNOPSIS #

```c
#include <libpmemset.h>

struct pmemset;
struct pmemset_map_config;
int pmemset_map_config_new(struct pmemset_map_config **map_cfg, struct pmemset *set);
int pmemset_map_config_delete(struct pmemset_map_config **map_cfg);
```

# DESCRIPTION #

The **pmemset_map_config_new**() creates new config based on the set specified in the *\*set* pointer.

If the **pmemset_map_config_new**() function succeeds in creating a new map config it
instantiates a new *struct pmemset_map_config** object describing the future mapping. The pointer
to this newly created object is stored in the user-provided variable passed
via the *map_cfg* pointer.

Other map config parameters which describe mapping can be set using **pmemset_map_config_set_source**(3),
**pmemset_map_config_set_length**(3) and **pmemset_map_config_set_offset**(3) functions.

The **pmemset_map_config_delete**() function frees *\*map_cfg* returned by **pmemset_map_config_new**()
and sets *\*map_cfg* to NULL.

# RETURN VALUE #

The **pmemset_map_config_new**() function returns 0 on success
or a negative error code on failure.

The **pmemset_map_config_delete**() function always returns 0.

# ERRORS #

The **pmemset_map_config_new**() can fail with the following errors:

* **-ENOMEM** in case of insufficient memory to allocate an instance
of *struct pmemset_map_config*.

# SEE ALSO #

**pmemset_map_config_set_length**(3), **pmemset_map_config_set_offset**(3),
**pmemset_map_config_set_source**(3), **libpmemset**(7) and **<http://pmem.io>**

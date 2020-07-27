---
layout: manual
Content-Style: 'text/css'
title: _MP(PMEM2_CONFIG_NEW, 3)
collection: libpmem2
header: PMDK
date: pmem2 API version 1.0
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2020, Intel Corporation)

[comment]: <> (pmem2_config_new.3 -- man page for pmem2_config_new and pmem2_config_delete)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmem2_config_new**(), **pmem2_config_delete**() - allocate and free a
configuration structure for a libpmem2 mapping

# SYNOPSIS #

```c
#include <libpmem2.h>

struct pmem2_config;
int pmem2_config_new(struct pmem2_config **cfg);
int pmem2_config_delete(struct pmem2_config **cfg);
```

# DESCRIPTION #

The **pmem2_config_new**() function instantiates a new (opaque) configuration structure, *pmem2_config*, which is used to define mapping parameters for a **pmem2_map_new**() function, and returns it through the pointer in *\*cfg*.

New configuration is always initialized with default values for all possible parameters, which are specified alongside the corresponding setter function.

The **pmem2_config_delete**() function frees *\*cfg* returned by **pmem2_config_new**() and sets *\*cfg* to NULL.
If *\*cfg* is NULL, no operation is performed.

# RETURN VALUE #

The **pmem2_config_new**() function returns 0 on success or a negative error code on failure.
**pmem2_config_new**() does set *\*cfg* to NULL on failure.

The **pmem2_config_delete**() function always returns 0.

# ERRORS #

**pmem2_config_new**() can fail with the following error:
- **-ENOMEM** - out of memory

# SEE ALSO #

**errno**(3), **pmem2_map_new**(3), **pmem2_config_set_handle**(3),
**pmem2_config_set_fd**(3), **pmem2_config_get_file_size**(3),
**libpmem2**(7) and **<http://pmem.io>**

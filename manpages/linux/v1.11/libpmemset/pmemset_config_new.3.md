---
layout: manual
Content-Style: 'text/css'
title: PMEMSET_CONFIG_NEW
collection: libpmemset
header: PMDK
date: pmemset API version 1.0
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2020, Intel Corporation)

[comment]: <> (pmemset_config_new.3 -- man page for pmemset_config_new and pmemset_config_delete)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[ERRORS](#errors)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmemset_config_new**(), **pmemset_config_delete**() - allocate and free a
configuration structure for a pmemset object

# SYNOPSIS #

```c
#include <libpmemset.h>

struct pmemset_config;
int pmemset_config_new(struct pmemset_config **cfg);
int pmemset_config_delete(struct pmemset_config **cfg);
```

# DESCRIPTION #

The **pmemset_config_new**() function instantiates a new (opaque) configuration structure,
*pmemset_config*, which is used to define *set* parameters for a **pmemset_new**(3) function,
and returns it through the pointer in *\*cfg*.

The **pmemset_config_delete**() function frees *\*cfg* returned by **pmemset_config_new**()
and sets *\*cfg* to NULL. If *\*cfg* is NULL, no operation is performed.

# RETURN VALUE #

The **pmemset_config_new**() function returns 0 on success or a negative error code on failure.
**pmemset_config_new**() does set *\*cfg* to NULL on failure.

The **pmemset_config_delete**() function always returns 0.

# ERRORS #

**pmemset_config_new**() can fail with the following error:
- **-ENOMEM** - out of memory

# SEE ALSO #

**errno**(3), **pmemset_new**(3),
**libpmemset**(7) and **<http://pmem.io>**

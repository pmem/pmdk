---
layout: manual
Content-Style: 'text/css'
title: _MP(PMEMSET_NEW, 3)
collection: libpmemset
header: PMDK
date: pmemset API version 1.0
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2020, Intel Corporation)

[comment]: <> (pmemset_new.3 -- man page for pmemset_new and pmemset_delete)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[ERRORS](#errors)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmemset_new**(), **pmemset_delete**() - allocate and free a structure for a
pmemset object

# SYNOPSIS #

```c
#include <libpmemset.h>

struct pmemset;
struct pmemset_config;
int pmemset_new(struct pmemset **set, struct pmemset_config *cfg);
int pmemset_delete(struct pmemset **set);
```

# DESCRIPTION #

The **pmemset_new**() function creates a new set by allocating and initializing set structure,
*pmemset*, and returns it through the pointer in *\*set*. Configuration data, passed by the *\*cfg*, is copied into the set structure.

The **pmemset_delete**() function frees *\*set* returned by **pmemset_new**()
and sets *\*set* to NULL. If *\*set* is NULL, no operation is performed.

# RETURN VALUE #

The **pmemset_new**() function returns 0 on success or a negative error code on failure.
**pmemset_new**() does set *\*set* to NULL on failure.

The **pmemset_delete**() function always returns 0.

# ERRORS #

**pmemset_new**() can fail with the following error:
- **-ENOMEM** - out of memory

# SEE ALSO #

**errno**(3), **pmemset_config_new**(3),
**libpmemset**(7) and **<http://pmem.io>**

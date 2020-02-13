---
layout: manual
Content-Style: 'text/css'
title: PMEMPOOL_RM
collection: libpmempool
header: PMDK
date: pmempool API version 1.3
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2017-2018, Intel Corporation)

[comment]: <> (pmempool_rm.3 -- man page for pool set management functions)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmempool_rm**() - remove persistent memory pool

# SYNOPSIS #

```c
#include <libpmempool.h>

int pmempool_rm(const char *path, int flags);
```



# DESCRIPTION #

The **pmempool_rm**() function removes the pool pointed to by *path*. The *path*
can point to a regular file, device dax or pool set file. If *path* is a pool
set file, **pmempool_rm**() will remove all part files from local replicas
using **unlink**(2), and all remote replicas using **rpmem_remove**(3)
(see **librpmem**(7)), before removing the pool set file itself.

The *flags* argument determines the behavior of **pmempool_rm**().
It is either 0 or the bitwise OR of one or more of the following flags:

+ **PMEMPOOL_RM_FORCE** - Ignore all errors when removing part files from
local or remote replicas.

+ **PMEMPOOL_RM_POOLSET_LOCAL** - Also remove local pool set file.

+ **PMEMPOOL_RM_POOLSET_REMOTE** - Also remove remote pool set file.

# RETURN VALUE #

On success, **pmempool_rm**() returns 0. On error, it returns -1 and sets
*errno* accordingly.

# SEE ALSO #

**rpmem_remove**(3), **unlink**(3), **libpmemlog**(7),
**libpmemobj**(7), **librpmem**(7) and **<https://pmem.io>**

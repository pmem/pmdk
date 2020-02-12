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

**pmempool_rmU**()/**pmempool_rmW**() - remove persistent memory pool

# SYNOPSIS #

```c
#include <libpmempool.h>

int pmempool_rmU(const char *path, int flags);
int pmempool_rmW(const wchar_t *path, int flags);
```


>NOTE: The PMDK API supports UNICODE. If the **PMDK_UTF8_API** macro is
defined, basic API functions are expanded to the UTF-8 API with postfix *U*.
Otherwise they are expanded to the UNICODE API with postfix *W*.

# DESCRIPTION #

The **pmempool_rmU**()/**pmempool_rmW**() function removes the pool pointed to by *path*. The *path*
can point to a regular file, device dax or pool set file. If *path* is a pool
set file, **pmempool_rmU**()/**pmempool_rmW**() will remove all part files from local replicas
using **unlink**(2) before removing the pool set file itself.

The *flags* argument determines the behavior of **pmempool_rmU**()/**pmempool_rmW**().
It is either 0 or the bitwise OR of one or more of the following flags:

+ **PMEMPOOL_RM_FORCE** - Ignore all errors when removing part files from
local replicas.

+ **PMEMPOOL_RM_POOLSET_LOCAL** - Also remove local pool set file.



# RETURN VALUE #

On success, **pmempool_rmU**()/**pmempool_rmW**() returns 0. On error, it returns -1 and sets
*errno* accordingly.

# SEE ALSO #

**rpmem_remove**(3), **unlink**(3), **libpmemlog**(7),
**libpmemobj**(7), **librpmem**(7) and **<https://pmem.io>**

---
draft: false
slider_enable: true
description: ""
disclaimer: "The contents of this web site and the associated <a href=\"https://github.com/pmem\">GitHub repositories</a> are BSD-licensed open source."
aliases: ["pmempool_rm.3.html"]
title: "libpmempool | PMDK"
header: "pmempool API version 1.3"
---

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2017-2022, Intel Corporation)

[comment]: <> (pmempool_rm.3 -- man page for pool set management functions)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />

# NAME #

_UW(pmempool_rm) - remove persistent memory pool

# SYNOPSIS #

```c
#include <libpmempool.h>

_UWFUNCR1(int, pmempool_rm, *path, int flags)
```

_UNICODE()

# DESCRIPTION #

The _UW(pmempool_rm) function removes the pool pointed to by *path*. The *path*
can point to a regular file, device dax or pool set file. If *path* is a pool
set file, _UW(pmempool_rm) will remove all part files from replicas
using **unlink**(2) before removing the pool set file itself.

The *flags* argument determines the behavior of _UW(pmempool_rm).
It is either 0 or the bitwise OR of one or more of the following flags:

+ **PMEMPOOL_RM_FORCE** - Ignore all errors when removing part files from replicas.

+ **PMEMPOOL_RM_POOLSET_LOCAL** - Also remove local pool set file.

# RETURN VALUE #

On success, _UW(pmempool_rm) returns 0. On error, it returns -1 and sets
*errno* accordingly.

# SEE ALSO #

**unlink**(3), **libpmemobj**(7) and **<https://pmem.io>**

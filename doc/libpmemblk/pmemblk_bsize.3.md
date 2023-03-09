---
draft: false
slider_enable: true
description: ""
disclaimer: "The contents of this web site and the associated <a href=\"https://github.com/pmem\">GitHub repositories</a> are BSD-licensed open source."
aliases: ["pmemblk_bsize.3.html"]
title: "libpmemblk | PMDK"
header: "pmemblk API version 1.1"
---

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2017-2023, Intel Corporation)

[comment]: <> (pmemblk_bsize.3 -- man page for functions that check number of available blocks or usable space in block memory pool)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmemblk_bsize**()(DEPRECATED), **pmemblk_nblock**()(DEPRECATED) - check number of available blocks or
usable space in block memory pool

# SYNOPSIS #

```c
#include <libpmemblk.h>

size_t pmemblk_bsize(PMEMblkpool *pbp);
size_t pmemblk_nblock(PMEMblkpool *pbp);
```

# DESCRIPTION #

The **pmemblk_bsize**() function returns the block size of the specified
block memory pool, that is, the value which was passed as *bsize* to
_UW(pmemblk_create). *pbp* must be a block memory pool handle as returned by
**pmemblk_open**(3) or **pmemblk_create**(3).

The **pmemblk_nblock**() function returns the usable space in the block memory
pool. *pbp* must be a block memory pool handle as returned by
**pmemblk_open**(3) or **pmemblk_create**(3).

# RETURN VALUE #

The **pmemblk_bsize**() function returns the block size of the specified block
memory pool.

The **pmemblk_nblock**() function returns the usable space in the block memory
pool, expressed as the number of blocks available.

# SEE ALSO #

**pmemblk_create**(3), **pmemblk_open**(3),
**libpmemblk**(7) and **<https://pmem.io>**

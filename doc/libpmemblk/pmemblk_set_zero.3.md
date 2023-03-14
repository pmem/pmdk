---
draft: false
slider_enable: true
description: ""
disclaimer: "The contents of this web site and the associated <a href=\"https://github.com/pmem\">GitHub repositories</a> are BSD-licensed open source."
aliases: ["pmemblk_set_zero.3.html"]
title: "libpmemblk | PMDK"
header: "pmemblk API version 1.1"
---

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2017-2023, Intel Corporation)

[comment]: <> (pmemblk_set_zero.3 -- man page for block management functions)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmemblk_set_zero**()(DEPRECATED), **pmemblk_set_error**()(DEPRECATED) - block management functions

# SYNOPSIS #

```c
#include <libpmemblk.h>

int pmemblk_set_zero(PMEMblkpool *pbp, long long blockno);
int pmemblk_set_error(PMEMblkpool *pbp, long long blockno);
```

# DESCRIPTION #

The **pmemblk_set_zero**() function writes zeros to block number *blockno* in
persistent memory resident array of blocks *pbp*. Using this function is faster
than actually writing a block of zeros since **libpmemblk**(7) uses metadata to
indicate the block should read back as zero.

The **pmemblk_set_error**() function sets the error state for block number
*blockno* in persistent memory resident array of blocks *pbp*.
A block in the error state returns *errno* **EIO** when read.
Writing the block clears the error state and returns the block to normal use.

# RETURN VALUE #

On success,  **pmemblk_set_zero**() and **pmemblk_set_error**() return 0.
On error, they return -1 and set *errno* appropriately.

# SEE ALSO #

**libpmemblk**(7) and **<https://pmem.io>**

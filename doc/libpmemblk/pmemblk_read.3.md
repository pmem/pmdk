---
draft: false
slider_enable: true
description: ""
disclaimer: "The contents of this web site and the associated <a href=\"https://github.com/pmem\">GitHub repositories</a> are BSD-licensed open source."
aliases: ["pmemblk_read.3.html"]
title: "libpmemblk | PMDK"
header: "pmemblk API version 1.1"
---

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2017-2023, Intel Corporation)

[comment]: <> (pmemblk_read.3 -- man page for libpmemblk read and write functions)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmemblk_read**()(DEPRECATED), **pmemblk_write**()(DEPRECATED) - read or write a block from a block
memory pool

# SYNOPSIS #

```c
#include <libpmemblk.h>

int pmemblk_read(PMEMblkpool *pbp, void *buf, long long blockno);
int pmemblk_write(PMEMblkpool *pbp, const void *buf, long long blockno);
```

# DESCRIPTION #

The **pmemblk_read**() function reads the block with block number *blockno*
from memory pool *pbp* into the buffer *buf*. Reading a block that has never
been written by **pmemblk_write**() will return a block of zeroes.

The **pmemblk_write**() function writes a block from *buf* to block number
*blockno* in the memory pool *pbp*. The write is atomic with respect to other
reads and writes. In addition, the write cannot be torn by program failure or
system crash; on recovery the block is guaranteed to contain either the old
data or the new data, never a mixture of both.

# RETURN VALUE #

On success, the **pmemblk_read**() and **pmemblk_write**() functions return 0.
On error, they return -1 and set *errno* appropriately.

# SEE ALSO #

**libpmemblk**(7) and **<https://pmem.io>**

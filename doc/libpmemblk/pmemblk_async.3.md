---
draft: false
slider_enable: true
description: ""
disclaimer: "The contents of this web site and the associated <a href=\"https://github.com/pmem\">GitHub repositories</a> are BSD-licensed open source."
aliases: ["pmemblk_async.3.html"]
title: "libpmemblk | PMDK"
header: "pmemblk API version 1.0"
---

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2022, Intel Corporation)

[comment]: <> (pmemblk_async.3 -- man page for pmemblk_async operations)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />

# NAME #
**pmemblk_xcreate**(), **pmemblk_xopen**(), **pmemblk_write_async**(), **pmemblk_read_async**() - asynchronous block operations

# SYNOPSIS #

```c
#define PMEMBLK_USE_MINIASYNC 1
#include <libpmemblk.h>

struct vdm;
struct pmemblk_write_async_future;
struct pmemblk_read_async_future;

PMEMblkpool *pmemblk_xcreate(const char *path, size_t bsize, size_t poolsize,
	mode_t mode, struct vdm *vdm);

PMEMblkpool *pmemblk_xopen(const char *path, size_t bsize, struct vdm *vdm);

struct pmemblk_write_async_future pmemblk_write_async(PMEMblkpool *pbp,
	void *buf, long long blockno);

struct pmemblk_read_async_future pmemblk_read_async(PMEMblkpool *pbp, void *buf,
	long long blockno);
```

# DESCRIPTION #

Asynchronous **libpmemblk**(7) functions require **libminiasync** library to be installed.
Those functions depend on the *future* and *vdm* (virtual data mover) concepts explained
in **miniasync**(7) documentation that is available on **<https://github.com/pmem/miniasync>**
repository.

The **pmemblk_xcreate** and **pmemblk_xopen** are extended versions of the standard
**pmemblk_create**(3) and **pmemblk_open**(3) functions. Those extended functions take
a *vdm* structure from **miniasync**(7) library as an argument. **libpmemblk**(7) library
uses *vdm* structure for every asynchronous operation. To work correctly, asynchronous
**libpmemblk**(7) functions require *PMEMblkpool* created with one of the extended functions.
For detailed information about *vdm*, see **miniasync_vdm**(7) on **miniasync**(7) repository.

The *pmemblk_write_async_future* and *pmemblk_read_async_future* structures describe
asynchronous block write and read tasks respectively. Those *futures* take into account
data persistence.

The **pmemblk_write_async** function initializes the *pmemblk_write_async_future*
*future* to write a block from *buf* to the block number *blockno* in the memory
pool *pbp*.

The **pmemblk_read_async** function initializes the *pmemblk_read_async_future*
*future* to read a block number *blockno* from the memory pool *pbp* to the buffer *buf*.

*Futures* can start execution by calling **future_poll**(3) function provided
by **miniasync**(7) library. Detailed information about *futures* is available in
**miniasync_future**(7) on **miniasync**(7) repository.

# RETURN VALUE #

The **pmemblk_xcreate** and **pmemblk_xopen** return a new instance of *PMEMblkpool*
that is required to perform asynchronous **libpmemblk**(7) functions.

The **pmemblk_write_async** returns a new instance of *pmemblk_write_async_future*
that describes an asynchronous write operation.

The **pmemblk_read_async** returns a new instance of *pmemblk_read_async_future*
that describes an asynchronous read operation.

# SEE ALSO #

**pmemblk_create**(3), **pmemblk_open**(3), **miniasync**(7),
**miniasync_future**(7), **miniasync_vdm**(7), **libpmemblk**(7),
**<https://github.com/pmem/miniasync>** and **<https://pmem.io>**


---
draft: false
slider_enable: true
description: ""
disclaimer: "The contents of this web site and the associated <a href=\"https://github.com/pmem\">GitHub repositories</a> are BSD-licensed open source."
aliases: ["rpmem_persist.3.html"]
title: "librpmem | PMDK"
header: "rpmem API version 1.3"
---

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2017-2022, Intel Corporation)

[comment]: <> (rpmem_persist.3 -- man page for rpmem persist, flush, drain and read functions)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[CAVEATS](#caveats)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**rpmem_persist**()(DEPRECATED), **rpmem_deep_persist**()(DEPRECATED),
**rpmem_flush**()(DEPRECATED), **rpmem_drain**()(DEPRECATED),
**rpmem_read**()(DEPRECATED) - functions to copy and read remote pools

# SYNOPSIS #

```c
#include <librpmem.h>

int rpmem_persist(RPMEMpool *rpp, size_t offset,
	size_t length, unsigned lane, unsigned flags);
int rpmem_deep_persist(RPMEMpool *rpp, size_t offset,
	size_t length, unsigned lane);

int rpmem_flush(RPMEMpool *rpp, size_t offset,
	size_t length, unsigned lane, unsigned flags);
int rpmem_drain(RPMEMpool *rpp, unsigned lane, unsigned flags);

int rpmem_read(RPMEMpool *rpp, void *buff, size_t offset,
	size_t length, unsigned lane);
```

# DESCRIPTION #

The **rpmem_persist**() function copies data of given *length* at given
*offset* from the associated local memory pool and makes sure the data is
persistent on the remote node before the function returns. The remote node
is identified by the *rpp* handle which must be returned from either
**rpmem_open**(3) or **rpmem_create**(3). The *offset* is relative
to the *pool_addr* specified in the **rpmem_open**(3) or **rpmem_create**(3)
call. If the remote pool was created using **rpmem_create**() with non-NULL
*create_attr* argument, *offset* has to be greater or equal to 4096.
In that case the first 4096 bytes of the pool is used for storing the pool
metadata and cannot be overwritten.
If the pool was created with NULL *create_attr* argument, the pool metadata
is not stored with the pool and *offset* can be any nonnegative number.
The *offset* and *length* combined must not exceed the
*pool_size* passed to **rpmem_open**(3) or **rpmem_create**(3).
The **rpmem_persist**() operation is performed using the given *lane* number.
The lane must be less than the value returned by **rpmem_open**(3) or
**rpmem_create**(3) through the *nlanes* argument (so it can take a value
from 0 to *nlanes* - 1). The *flags* argument can be 0 or RPMEM_PERSIST_RELAXED
which means the persist operation will be done without any guarantees regarding
atomicity of memory transfer.

The **rpmem_deep_persist**() function works in the same way as
**rpmem_persist**(3) function, but additionally it flushes the data to the
lowest possible persistency domain available from software.
Please see **pmem_deep_persist**(3) for details.

The **rpmem_flush**() and **rpmem_drain**() functions are two halves of the
single **rpmem_persist**(). The **rpmem_persist**() copies data and makes it
persistent in the one shot, where **rpmem_flush**() and **rpmem_drain**() split
this operation into two stages. The **rpmem_flush**() copies data of given
*length* at a given *offset* from the associated local memory pool to the
remote node. The **rpmem_drain**() makes sure the data copied in all preceding
**rpmem_flush**() calls is persistent on the remote node before the function
returns. Data copied using **rpmem_flush**() can not be considered persistent
on the remote node before return from following **rpmem_drain**().
Single **rpmem_drain**() confirms persistence on the remote node of data copied
by all **rpmem_flush**() functions called before it and using the same *lane*.
The last **rpmem_flush**() + **rpmem_drain**() can be replaced with
**rpmem_persist**() at no cost.

The *flags* argument for **rpmem_flush**() can be 0 or RPMEM_FLUSH_RELAXED
which means the flush operation will be done without any guarantees regarding
atomicity of memory transfer. The *flags* argument for **rpmem_drain**() must be 0.

The **rpmem_flush**() function performance is affected by **RPMEM_WORK_QUEUE_SIZE**
environment variable (see **librpmem**(7) for more details).

The **rpmem_read**() function reads *length* bytes of data from a remote pool
at *offset* and copies it to the buffer *buff*. The operation is performed on
the specified *lane*. The lane must be less than the value returned by
**rpmem_open**(3) or **rpmem_create**(3) through the *nlanes* argument
(so it can take a value from 0 to *nlanes* - 1). The *rpp* must point to a
remote pool opened or created previously by **rpmem_open**(3) or
**rpmem_create**(3).

# RETURN VALUE #

The **rpmem_persist**() function returns 0 if the entire memory area was
made persistent on the remote node. Otherwise it returns a non-zero value
and sets *errno* appropriately.

The **rpmem_flush**() function returns 0 if duplication of the memory area to
the remote node was initialized successfully. Otherwise, it returns a non-zero
value and sets *errno* appropriately.

The **rpmem_drain**() function returns 0 if the memory areas duplicated by all
**rpmem_flush**() calls preceding the **rpmem_drain**() are made persistent
on the remote node. Otherwise, it returns a non-zero value and sets *errno*
appropriately.

The **rpmem_read**() function returns 0 if the data was read entirely.
Otherwise it returns a non-zero value and sets *errno* appropriately.

# CAVEATS #

Ordering of **rpmem_flush**() and **rpmem_persist**() operations which are using
different *lane* values is not guaranteed.

# SEE ALSO #

**rpmem_create**(3), **rpmem_open**(3), **rpmem_persist**(3),
**sysconf**(3), **limits.conf**(5), **libpmemobj**(7)
and **<https://pmem.io>**

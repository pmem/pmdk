---
draft: false
slider_enable: true
description: ""
disclaimer: "The contents of this web site and the associated <a href=\"https://github.com/pmem\">GitHub repositories</a> are BSD-licensed open source."
aliases: ["pmem2_async.3.html"]
title: "libpmem2 | PMDK"
header: "pmem2 API version 1.0"
---

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2022, Intel Corporation)

[comment]: <> (pmem2_async.3 -- man page for pmem2_async operations)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />

# NAME #
**pmem2_config_set_vdm**()(DEPRECATED), **pmem2_memcpy_async**()(DEPRECATED), **pmem2_memmove_async**()(DEPRECATED),
**pmem2_memset_async**()(DEPRECATED) - asynchronous data movement operations

> NOTICE:
Support for async functions is deprecated since PMDK 1.13.0 release
and will be removed in the PMDK 2.0.0 release along with the miniasync dependency.

# SYNOPSIS #

```c
#define PMEM2_USE_MINIASYNC 1
#include <libpmem2.h>
struct pmem2_future;

int pmem2_config_set_vdm(struct pmem2_config *cfg, struct vdm *vdm);

struct pmem2_future pmem2_memcpy_async(struct pmem2_map *map,
	void *pmemdest, const void *src, size_t len, unsigned flags);

struct pmem2_future pmem2_memmove_async(struct pmem2_map *map, void *pmemdest, const void *src,
	size_t len, unsigned flags)

struct pmem2_future pmem2_memset_async(struct pmem2_map *map,
	void *pmemstr,	int c, size_t n, unsigned flags)
{
```

# DESCRIPTION #
To use those functions, you must have *libminiasync* installed. Those functions use futures
and vdm (virtual data mover) concepts from this library. Please check **miniasync**(7) for more details.

The struct **pmem2_future** is a structure describing a task to be done asynchronously taking into account persistence
of the operation. It means that by the time the future is complete, all the data is safely written into a persistent domain.

The **pmem2_config_set_vdm** sets a vdm structure in the *pmem2_config*.
This structure will be used by pmem2_*_async functions, to create a *pmem2_future*.
If vdm is not set in the config, pmem2_map_new will use a default one which uses a
pmem2 memory movement functions to perform memory operations. (**pmem2_get_memcpy_fn**(3), **pmem2_get_memmove_fn**(3), **pmem2_get_memset_fn**(3)).

The **pmem2_memcpy_async** uses *vdm* structure held inside the *pmem2_map* structure to initialise and returns **pmem2_future**.
This future will perform memcpy operation defined in *vdm* to copy *len* bytes from *src* to *pmemdest*. In the current implementation *flags* are ignored.

The **pmem2_memmove_async** returns **pmem2_future** which
will perform memmove operation defined in *vdm* to copy *len* bytes from *src* to *pmemdest*. In the current implementation *flags* are ignored.

The **pmem2_memset_async** returns **pmem2_future** which
will perform memset operation defined in *vdm* to fill *n* bytes from *pmemstr* with value of int *c* interpreted as unsigned char.
In the current implementation *flags* are ignored.

# RETURN VALUE #
The **pmem2_config_set_vdm** always return 0.

The **pmem2_memcpy_async** returns a new instance of **pmem2_future** performing memcpy operation.
You can execute returned structure using methods from the **libminiasync**() library such as **FUTURE_BUSY_POLL**(3).

The **pmem2_memmove_async** returns a new instance of **pmem2_future** performing memmove operation.

The **pmem2_memset_async** returns a new instance of **pmem2_future** performing memset operation.

# SEE ALSO #

**memcpy**(3), **memmove**(3), **memset**(3), **pmem2_get_drain_fn**(3),
**pmem2_get_memcpy_fn**(3), **pmem2_get_memset_fn**(3), **pmem2_map_new**(3),
**pmem2_get_persist_fn**(3), **vdm_memcpy**(3), **miniasync**(7), **miniasync_future**(7),
**libpmem2**(7) and **<https://pmem.io>**

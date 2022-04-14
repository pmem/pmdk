---
layout: manual
Content-Style: 'text/css'
title: _MP(PMEM2_ASYNC, 3)
collection: libpmem2
header: PMDK
date: pmem2 API version 1.0
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause
[comment]: <> (Copyright 2022, Intel Corporation)

[comment]: <> (pmem2_async.3 -- man page for pmem2_async operations)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />

# NAME #
**pmem2_config_set_vdm**(), **pmem2_memcpy_async**(), **pmem2_memmove_async**(), **pmem2_memset_async**() - asynchronous data movement operations

# SYNOPSIS #

```c
#define PMEM2_USE_MINIASYNC 1
#include <libpmem2.h>
int pmem2_config_set_vdm(struct pmem2_config *cfg, struct vdm *vdm);

struct vdm_operation_future pmem2_memcpy_async(struct pmem2_map *map,
	void *pmemdest, const void *src, size_t len, unsigned flags);

pmem2_memmove_async(struct pmem2_map *map, void *pmemdest, const void *src,
	size_t len, unsigned flags)

struct vdm_operation_future pmem2_memset_async(struct pmem2_map *map,
	void *pmemstr,	int c, size_t n, unsigned flags)
{
```

# DESCRIPTION #
To use those functions, you must have *libminiasync* installed. Those functions use futures
and vdm (virtual data mover) concepts from this library. Please check **miniasync**(7) for more details.

The **pmem2_config_set_vdm** sets a vdm structure in the *pmem2_config*.
This structure will be used by pmem2_*_async functions, to create a *vdm_operation_future*.
If vdm is not set in the config, pmem2_map_new will use a default one which uses a
pmem2 memory movement functions to perform memory operations. (**pmem2_get_memcpy_fn**(3), **pmem2_get_memmove_fn**(3), **pmem2_get_memsety_fn**(3)).

The **pmem2_memcpy_async** uses *vdm* structure held inside the *pmem2_map* structure to initialise and returns **vdm_operation_future**.
This future will perform memcpy operation defined in *vdm* to copy *len* bytes from *src* to *pmemdest*. In the current implementation *flags* are ignored.

The **pmem2_memmove_async** returns **vdm_operation_future** which
will perform memmove operation defined in *vdm* to copy *len* bytes from *src* to *pmemdest*. In the current implementation *flags* are ignored.

The **pmem2_memmset_async** returns **vdm_operation_future** which
will perform memset operation defined in *vdm* to fill *n* bytes from *pmemstr* with value of int *c* interpreted as unsigned char.
In the current implementation *flags* are ignored.

# RETURN VALUE #
The **pmem2_config_set_vdm** always return 0.

The **pmem2_memcpy_async** returns a new instance of **vdm_operation_future** performing memcpy operation.
You can execute returned structure using methods from the **libminiasync**() library such as **FUTURE_BUSY_POLL**(3).

The **pmem2_memmove_async** returns a new instance of **vdm_operation_future** performing memmove operation.

The **pmem2_memset_async** returns a new instance of **vdm_operation_future** performing memset operation.

# SEE ALSO #

**memcpy**(3), **memmove**(3), **memset**(3), **pmem2_get_drain_fn**(3),
**pmem2_get_memcpy_fn**(3), **pmem2_get_memset_fn**(3), **pmem2_map_new**(3),
**pmem2_get_persist_fn**(3), **vdm_memcpy**(3), **miniasync**(7), **miniasync_future**(7),
**libpmem2**(7) and **<https://pmem.io>**


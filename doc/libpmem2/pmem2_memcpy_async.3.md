---
layout: manual
Content-Style: 'text/css'
title: _MP(PMEM2_MEMCPY_ASYNC, 3)
collection: libpmem2
header: PMDK
date: pmem2 API version 1.0
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause
[comment]: <> (Copyright 2022, Intel Corporation)

[comment]: <> (pmem2_memcpy_async.3 -- man page for pmem2_memcpy_async)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />

# NAME #
**pmem2_config_set_vdm**(), **pmem2_memcpy_async**() - asynchronous data movement operations

# SYNOPSIS #

```c
#define PMEM2_USE_MINIASYNC 1
#include <libpmem2.h>
int pmem2_config_set_vdm(struct pmem2_config *cfg, struct vdm *vdm);

struct vdm_operation_future pmem2_memcpy_async(struct pmem2_map *map,
	void *pmemdest, const void *src, size_t len, unsigned flags);
```

# DESCRIPTION #
To use those functions, you must have *libminiasync* installed. Those functions use futures
and vdm (virtual data mover) concepts from this library. Please check **miniasync**(7) for more details.

The **pmem2_config_set_vdm** sets a vdm structure in the *pmem2_config*.
This structure will be used by pmem2_memcpy_async function, to create a *vdm_operation_future*.
If vdm is not set in the config, pmem2_map_new will use a default one which uses a
pmem2 memory movement functions to perform memory operations. (**pmem2_get_memcpy_fn**(3), **pmem2_get_memmove_fn**(3), **pmem2_get_memsety_fn**(3)).

The **pmem2_memcpy_async** uses *vdm* structure held inside of the *pmem2_map* structure to initialise and return **vdm_operation_future**.
This future will perform memcpy operation from *vdm* to copy *len* bytes from *src* to *pmemdest*. In the current implementation *flags* are ignored.

# RETURN VALUE #

The **pmem2_config_set_vdm** always return 0.

The **pmem2_memcpy_async** returns a new instance of **vdm_operation_future** which will copy *len* bytes from *src* to *pmemdest*.
You can execute returned structure using methods from the **libminiasync**() library.

# SEE ALSO #

**memcpy**(3), **memmove**(3), **memset**(3), **pmem2_get_drain_fn**(3),
**pmem2_get_memcpy_fn**(3), **pmem2_get_memset_fn**(3), **pmem2_map_new**(3),
**pmem2_get_persist_fn**(3), **vdm_memcpy**(3), **miniasync**(7), **miniasync_future**(7),
**libpmem2**(7) and **<https://pmem.io>**


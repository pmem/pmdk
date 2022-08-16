---
layout: manual
Content-Style: 'text/css'
title: _MP(VDM_IS_SUPPORTED, 3)
collection: miniasync
header: VDM_IS_SUPPORTED
secondary_title: miniasync
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2022, Intel Corporation)

[comment]: <> (vdm_is_supported.3 -- man page for miniasync vdm_is_supported operation)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**vdm_is_supported**() - returns information if a given capability is supported

# SYNOPSIS #

```c
#include <libminiasync.h>

static inline int vdm_is_supported(struct vdm *vdm, unsigned capability);

```

# DESCRIPTION #
**vdm_is_supported**() verifies if the given *vdm* supports a given flag, or other capability.

Currently vdm defines the following capabilities:
- **VDM_F_NO_CACHE_HINT** - If supported, user can pass this flag to the **vdm_memcpy**(), **vdm_memset**(), **vdm_memmove**()
functions, to hint vdm to bypass CPU cache, and write the data directly to the memory. If not supported vdm will ignore this flag.
- **VDM_F_MEM_DURABLE** -- If supported, user can pass this flag to the **vdm_memcpy**(), **vdm_memset**(), **vdm_memmove**() functions
to ensure that the data written has become persistent, when a future completes.

## RETURN VALUE ##

The **vdm_is_supported**() function returns nonzero if the given capability is supported, or zero otherwise.

# SEE ALSO #

**vdm_flush**(3), **vdm_memcpy**(3), **vdm_memmove**(3), **vdm_memset**(3), **miniasync**(7), **miniasync_vdm**(7),
**miniasync_vdm_dml**(7) and **<https://pmem.io>**

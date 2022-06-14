---
layout: manual
Content-Style: 'text/css'
title: _MP(VDM_MEMMOVE, 3)
collection: miniasync
header: VDM_MEMMOVE
secondary_title: miniasync
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2022, Intel Corporation)

[comment]: <> (vdm_memmove.3 -- man page for miniasync vdm_memmove operation)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**vdm_memmove**() - create a new memmove virtual data mover operation structure

# SYNOPSIS #

```c
#include <libminiasync.h>

enum vdm_operation_type {
	VDM_OPERATION_MEMCPY,
	VDM_OPERATION_MEMMOVE,
	VDM_OPERATION_MEMSET,
};

struct vdm_operation_output_memmove {
	void *dest;
};

FUTURE(vdm_operation_future,
	struct vdm_operation_data, struct vdm_operation_output);

struct vdm_operation_future vdm_memmove(struct vdm *vdm, void *dest, void *src,
	size_t n, uint64_t flags);
```

For general description of virtual data mover API, see **miniasync_vdm**(7).

# DESCRIPTION #

**vdm_memmove**() initializes and returns a new memmove future based on the virtual data mover
implementation instance *vdm*. The parameters: *dest*, *src*, *n* are standard memmove parameters.
The *flags* represents data mover specific flags. TODO: provide an example of the flags usage after
implementing memmove future for dml.

Memmove future obtained using **vdm_memmove**() will attempt to move *n* bytes from memory area
*src* to memory area *dest* when its polled.

## RETURN VALUE ##

The **vdm_memmove**() function returns an initialized *struct vdm_operation_future* memmove future.

# SEE ALSO #

**vdm_memcpy**(3), **vdm_memset**(3), **miniasync**(7), **miniasync_vdm**(7),
**miniasync_vdm_dml**(7) and **<https://pmem.io>**

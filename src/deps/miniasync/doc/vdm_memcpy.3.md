---
layout: manual
Content-Style: 'text/css'
title: _MP(VDM_MEMCPY, 3)
collection: miniasync
header: VDM_MEMCPY
secondary_title: miniasync
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2022, Intel Corporation)

[comment]: <> (vdm_memcpy.3 -- man page for miniasync vdm_memcpy operation)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**vdm_memcpy**() - create a new memcpy virtual data mover operation structure

# SYNOPSIS #

```c
#include <libminiasync.h>

struct vdm_operation_output_memcpy {
	void *dest;
};

FUTURE(vdm_operation_future,
	struct vdm_operation_data, struct vdm_operation_output);

struct vdm_operation_future vdm_memcpy(struct vdm *vdm, void *dest, void *src,
	size_t n, uint64_t flags);
```

For general description of virtual data mover API, see **miniasync_vdm**(7).

# DESCRIPTION #

**vdm_memcpy**() initializes and returns a new memcpy future based on the virtual data mover
implementation instance *vdm*. The parameters: *dest*, *src*, *n* are standard memcpy parameters.
The *flags* represents data mover specific flags. For example, **miniasync_vdm_dml**(7) flag
**VDM_F_MEM_DURABLE** specifies that the write destination is identified as a write
to durable memory. This flag is meant to be used only with the **miniasync_vdm_dml**(7) data mover
implementation, providing it to any other data mover will result in undefined behavior.

Memcpy future obtained using **vdm_memcpy**() will attempt to copy *n* bytes from memory area
*src* to memory area *dest* when its polled.

## RETURN VALUE ##

The **vdm_memcpy**() function returns an initialized *struct vdm_operation_future* memcpy future.

# SEE ALSO #

**vdm_flush**(3), **vdm_memmove**(3), **vdm_memset**(3), **miniasync**(7), **miniasync_vdm**(7),
**miniasync_vdm_dml**(7) and **<https://pmem.io>**

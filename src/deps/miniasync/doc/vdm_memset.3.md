---
layout: manual
Content-Style: 'text/css'
title: _MP(VDM_MEMSET, 3)
collection: miniasync
header: VDM_MEMSET
secondary_title: miniasync
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2022, Intel Corporation)

[comment]: <> (vdm_memset.3 -- man page for miniasync vdm_memset operation)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**vdm_memset**() - create a new memset virtual data mover operation structure

# SYNOPSIS #

```c
#include <libminiasync.h>

struct vdm_operation_output_memset {
	void *str;
};

FUTURE(vdm_operation_future,
	struct vdm_operation_data, struct vdm_operation_output);

struct vdm_operation_future vdm_memset(struct vdm *vdm, void *str, int c,
	size_t n, uint64_t flags);
```

For general description of virtual data mover API, see **miniasync_vdm**(7).

# DESCRIPTION #

**vdm_memset**() initializes and returns a new memset future based on the virtual data mover
implementation instance *vdm*. The parameters: *str*, *c*, *n* are standard memset parameters.
The *flags* represents data mover specific flags. For example, **miniasync_vdm_dml**(7) flag
**VDM_F_MEM_DURABLE** specifies that the write destination is identified as a write to
durable memory. This flag is meant to be used only with the **miniasync_vdm_dml**(7) data mover
implementation, providing it to any other data mover will result in undefined behavior.

Memset future obtained using **vdm_memset**() will attempt to copy the character *c* to the
first, *n* bytes of the memory area *str* when its polled.

## RETURN VALUE ##

The **vdm_memset**() function returns an initialized *struct vdm_operation_future* memset future.

# SEE ALSO #

**vdm_flush**(3), **vdm_memcpy**(3), **vdm_memmove**(3), **miniasync**(7), **miniasync_vdm**(7),
**miniasync_vdm_dml**(7) and **<https://pmem.io>**

---
layout: manual
Content-Style: 'text/css'
title: _MP(DATA_MOVER_SYNC_NEW, 3)
collection: miniasync
header: DATA_MOVER_SYNC_NEW
secondary_title: miniasync
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2022, Intel Corporation)

[comment]: <> (data_mover_sync_new.3 -- man page for miniasync data_mover_sync_new operation)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**data_mover_sync_new**(), **data_mover_sync_delete**() - allocate or free synchronous
data mover structure

# SYNOPSIS #

```c
#include <libminiasync.h>

struct data_mover_sync;

struct data_mover_sync *data_mover_sync_new(void);
void data_mover_sync_delete(struct data_mover_sync *dms);
```

For general description of synchronous data mover API, see **miniasync_vdm_synchronous**(7).

# DESCRIPTION #

The **data_mover_sync_new**() function allocates and initializes a new synchronous data mover structure.

The **data_mover_sync_delete**() function frees and finalizes the synchronous data mover structure
pointed by *dms*.

# RETURN VALUE #

The **data_mover_sync_new**() function returns a pointer to *struct data_mover_sync* structure or
**NULL** if the allocation or initialization failed.

The **data_mover_sync_delete**() function does not return any value.

# SEE ALSO #

**miniasync**(7), **miniasync_vdm_synchronous**(7)
and **<https://pmem.io>**

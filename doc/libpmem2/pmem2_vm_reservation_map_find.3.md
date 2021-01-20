---
layout: manual
Content-Style: 'text/css'
title: _MP(PMEM2_VM_RESERVATION_MAP_FIND, 3)
collection: libpmem2
header: PMDK
date: pmem2 API version 1.0
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2020, Intel Corporation)

[comment]: <> (pmem2_vm_reservation_map_find.3 -- man page for libpmem2 pmem2_vm_reservation_map_find operation)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmem2_vm_reservation_map_find**() - finds the mapping stored in the virtual
memory reservation

# SYNOPSIS #

```c
#include <libpmem2.h>

struct pmem2_vm_reservation;
struct pmem2_map *pmem2_vm_reservation_map_find(
		struct pmem2_vm_reservation *rsv,
		size_t reserv_offset, size_t len);
```

# DESCRIPTION #

The **pmem2_vm_reservation_map_find**() function searches for the earliest mapping,
stored in the virtual memory reservation, intersecting with the interval designated
by *reserv_offset* and *len* variables.

# RETURN VALUE #

The **pmem2_vm_reservation_map_find**() returns a mapping on successful search
or NULL when no mapping was found.

# SEE ALSO #

**pmem2_config_set_vm_reservation**(3), **libpmem2**(7) and **<http://pmem.io>**

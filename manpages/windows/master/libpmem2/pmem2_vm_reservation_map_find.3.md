---
layout: manual
Content-Style: 'text/css'
title: PMEM2_VM_RESERVATION_MAP_FIND
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
[ERRORS](#errors)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmem2_vm_reservation_map_find**() - finds the mapping stored in the virtual
memory reservation

# SYNOPSIS #

```c
#include <libpmem2.h>

struct pmem2_vm_reservation;
int pmem2_vm_reservation_map_find(struct pmem2_vm_reservation *rsv,
		size_t reserv_offset, size_t len, struct pmem2_map **map);
```

# DESCRIPTION #

The **pmem2_vm_reservation_map_find**() function searches for the earliest mapping,
stored in the virtual memory reservation, intersecting with the interval designated
by *reserv_offset* and *len* variables.

# RETURN VALUE #

The **pmem2_vm_reservation_map_find**() returns 0 on success or a negative error on failure.

It passes an address to the found mapping via user provided *map* pointer variable
on success, otherwise it passes *NULL* value when no mapping was found.

# ERRORS #

**pmem2_vm_reservation_map_find**() can fail with the following errors:

- **PMEM2_E_MAPPING_NOT_FOUND** - no mapping found in the region of the vm reservation
specified by *reserv_offset* and *len* variables.

# SEE ALSO #

**libpmem2**(7), and **<http://pmem.io>**

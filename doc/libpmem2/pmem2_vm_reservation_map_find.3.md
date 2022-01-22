---
layout: manual
Content-Style: 'text/css'
title: _MP(PMEM2_VM_RESERVATION_MAP_FIND, 3)
collection: libpmem2
header: PMDK
date: pmem2 API version 1.0
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2021-2022, Intel Corporation)

[comment]: <> (pmem2_vm_reservation_map_find.3 -- man page for libpmem2 pmem2_vm_reservation_map_find operation)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[ERRORS](#errors)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmem2_vm_reservation_map_find**(), **pmem2_vm_reservation_map_find_prev**(),
**pmem2_vm_reservation_map_find_next**(), **pmem2_vm_reservation_map_find_first**() and
**pmem2_vm_reservation_map_find_last**() - search for the mapping located at the
desirable location

# SYNOPSIS #

```c
#include <libpmem2.h>

struct pmem2_map;
struct pmem2_vm_reservation;
int pmem2_vm_reservation_map_find(struct pmem2_vm_reservation *rsv,
		size_t reserv_offset, size_t len, struct pmem2_map **map_ptr);
int pmem2_vm_reservation_map_find_prev(struct pmem2_vm_reservation *rsv,
		struct pmem2_map *map, struct pmem2_map **prev_map);
int pmem2_vm_reservation_map_find_next(struct pmem2_vm_reservation *rsv,
		struct pmem2_map *map, struct pmem2_map **next_map);
int pmem2_vm_reservation_map_find_first(struct pmem2_vm_reservation *rsv,
		struct pmem2_map **first_map);
int pmem2_vm_reservation_map_find_last(struct pmem2_vm_reservation *rsv,
		struct pmem2_map **last_map);
```

# DESCRIPTION #
Mappings are inserted to the virtual memory reservation in the order of their virtual
address space location. First mapping represents the earliest mapping in the virtual
address space contained in a reservation, whereas the last mapping represents the last one.

The **pmem2_vm_reservation_map_find**() function searches for the earliest mapping,
stored in the virtual memory reservation, intersecting with the interval defined by
*reserv_offset* and *len* variables and returns it via *map_ptr* variable.

**pmem2_vm_reservation_map_find_prev**() function searches for the map previous
to the provided *map* and returns it via provided *prev_map* variable.

**pmem2_vm_reservation_map_find_next**() function searches for the map next after
the provided *map* and returns it via *next_map* variable.

**pmem2_vm_reservation_map_find_first**() function searches for the first map in
the reservation and returns it via provided *first_map* variable.

**pmem2_vm_reservation_map_find_last**() function searches for the last map in
the reservation and returns it via provided *last_map* variable.
# RETURN VALUE #

The **pmem2_vm_reservation_map_find**(), **pmem2_vm_reservation_map_find_prev**(),
**pmem2_vm_reservation_map_find_next**(), **pmem2_vm_reservation_map_find_first**() and
**pmem2_vm_reservation_map_find_last**() return 0 on success or a negative error on failure.

It passes an address to the found mapping via user provided *map* pointer variable
on success, otherwise it passes *NULL* value when no mapping was found.

# ERRORS #

The **pmem2_vm_reservation_map_find**(), **pmem2_vm_reservation_map_find_prev**(),
**pmem2_vm_reservation_map_find_next**(), **pmem2_vm_reservation_map_find_first**() and
**pmem2_vm_reservation_map_find_last**() can fail with the following errors:

- **PMEM2_E_MAPPING_NOT_FOUND** - no mapping found at the desirable location of the reservation

# SEE ALSO #

**libpmem2**(7), and **<https://pmem.io>**

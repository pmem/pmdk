---
layout: manual
Content-Style: 'text/css'
title: _MP(PMEM2_VM_RESERVATION_REMOVE_RANGE, 3)
collection: libpmem2
header: PMDK
date: pmem2 API version 1.0
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2021, Intel Corporation)

[comment]: <> (pmem2_vm_reservation_remove_range.3 -- man page for libpmem2 pmem2_vm_reservation_remove_range operation)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[ERRORS](#errors)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmem2_vm_reservation_remove_range**() - removes existing mappings belonging to the
virtual memory reservation

# SYNOPSIS #

```c
#include <libpmem2.h>

struct pmem2_vm_reservation;
int pmem2_vm_reservation_remove_range(struct pmem2_vm_reservation **rsv,
		size_t offset, size_t size,
		struct pmem2_vm_reservation **new_rsv);
```

# DESCRIPTION #

**pmem2_vm_reservation_remove_range**() - removes and unmaps existing mappings, belonging to the virtual
memory reservation overlapping with provided range described by the *offset* and *size* variables.
Every mapping stored in the reservation that overlaps with the provided range will be unmapped. Reserved
memory region belonging to the *rsv* reservation, spanning from the range occupied by the earliest
overlapped mapping to the latest one, will be discarded and will no longer belong to the *rsv*.

In case that the range to be removed will sever the provided reservation in two parts, a new reservation
will be created and passed via *new_rsv* variable. New reservation will contain the mappings contained
in the severed part of the reservation situated later in the virtual memory range. The mapping contained
in the severed part of the reservation situated earlier in the virtual memory range will remain in the
reservation passed via *rsv* variable.

If the provided range overlapped with the mappings situated at the start and the end of the reservation
then the whole reservation will be removed and the *rsv* variable will be set to NULL.

# RETURN VALUE #

The **pmem2_vm_reservation_remove_range**() function returns 0 on success or a
negative error code on failure.

# ERRORS #

The **pmem2_vm_reservation_remove_range**() function can fail with the following errors:

* **PMEM2_E_MAPPING_NOT_FOUND** - the provided range doesn't overlap with any mapping stored
in the reservation

On systems other than Windows it can also return **-EINVAL** from the underlying
**munmap**(2) function.

# SEE ALSO #

**pmem2_config_set_vm_reservation**(3), **pmem2_vm_reservation_new**(3),
**libpmem2**(7) and **<http://pmem.io>**

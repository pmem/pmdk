---
layout: manual
Content-Style: 'text/css'
title: _MP(PMEM2_VM_RESERVATION_EXTEND, 3)
collection: libpmem2
header: PMDK
date: pmem2 API version 1.0
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2021, Intel Corporation)

[comment]: <> (pmem2_vm_reservation_extend.3 -- man page for libpmem2 pmem2_vm_reservation_extend and pmem2_vm_reservation_shrink operations)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[ERRORS](#errors)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmem2_vm_reservation_extend**(), **pmem2_vm_reservation_shrink**() - extends and shrinks
existing virtual memory reservation

# SYNOPSIS #

```c
#include <libpmem2.h>

struct pmem2_vm_reservation;
int pmem2_vm_reservation_extend(struct pmem2_vm_reservation *rsv, size_t size);
int pmem2_vm_reservation_shrink(struct pmem2_vm_reservation *rsv, size_t offset,
		size_t size);
```

# DESCRIPTION #

The **pmem2_vm_reservation_extend**() function extends an existing virtual memory
reservation by the given *size*. For the function to succeed the size has to be aligned
to an appropriate allocation granularity.

If the **pmem2_vm_reservation_extend**() succeeds in extending a reservation, it provides
placeholder virtual memory range that starts from an address at the end of the old reservation.
Mappings made to the reservation before extending are preserved.

The **pmem2_vm_reservation_shrink**() function shrinks the reservation by a region specified
by *offset* into the reservation and the *size*. For the function to succeed the *size* and
*offset* variables have to be aligned to an appropriate allocation granularity. The region
formed by *offset* and *size* has to belong to the reservation, be empty and it needs to cover
the beggining or the end of the reservation.
Shrinking reservation from the middle or shrinking the whole reservation is not supported.

If the **pmem2_vm_reservation_shrink**() succeeds in shrinking a reservation, it releases
placeholder virtual memory range that was designated by *offset* and *size* variables.
Mappings made to the reservation before shrinking are preserved.

If either of those functions fails, reservation will be left as it was and appropriate error
value will be returned.

# RETURN VALUE #

The **pmem2_vm_reservation_extend**() and **pmem2_vm_reservation_shrink**() functions return 0
on success or a negative error code on failure.

# ERRORS #

The **pmem2_vm_reservation_extend**() function can fail with the following errors:

* **PMEM2_E_MAPPING_EXISTS** - the range that the reservation would be extended by is already occupied
by an existing mapping.

It can also return **-EAGAIN**, **-ENOMEM** from the underlying **mmap**(2) function.

The **pmem2_vm_reservation_shrink**() function can fail with the following errors:

* **PMEM2_E_OFFSET_UNALIGNED** - provided offset isn't aligned to an appropriate allocation granularity.

* **PMEM2_E_LENGTH_UNALIGNED** - provided size isn't aligned to an appropriate allocation granularity.

* **PMEM2_E_OFFSET_OUT_OF_RANGE** - provided offset is out of reservation range available to be shrunk.

* **PMEM2_E_LENGTH_OUT_OF_RANGE** - provided size is out of reservation range available to be shrunk.

* **PMEM2_E_NOSUPP** - interval designated by *offset* and *size* variables covers only the
middle or the whole reservation range.

* **PMEM2_VM_RESERVATION_NOT_EMPTY** - interval designated by *offset* and *size* variable is not
empty.

It can also return **-EAGAIN** and **-ENOMEM** from the underlying **munmap**(2) function.

# SEE ALSO #

**pmem2_vm_reservation_new**(3), **pmem2_config_set_vm_reservation**(3),
**libpmem2**(7) and **<http://pmem.io>**

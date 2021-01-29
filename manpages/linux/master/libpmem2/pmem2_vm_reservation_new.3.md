---
layout: manual
Content-Style: 'text/css'
title: PMEM2_VM_RESERVATION_NEW
collection: libpmem2
header: PMDK
date: pmem2 API version 1.0
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2020-2021, Intel Corporation)

[comment]: <> (pmem2_vm_reservation_new.3 -- man page for libpmem2 virtual memory reservation API)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[ERRORS](#errors)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmem2_vm_reservation_new**(), **pmem2_vm_reservation_delete**() - creates or deletes
an instance of virtual memory reservation

# SYNOPSIS #

```c
#include <libpmem2.h>

struct pmem2_vm_reservation;
void pmem2_vm_reservation_new(struct pmem2_vm_reservation **rsv_ptr,
		void *addr, size_t size);
void pmem2_vm_reservation_delete(struct pmem2_vm_reservation **rsv_ptr);
```

# DESCRIPTION #

The **pmem2_vm_reservation_new**() function creates a new blank mapping in the
virtual address space of the calling process. Reservation serves as a placeholder
of a given size on which sources can be mapped.

For the function to succeed, the *addr* must be either aligned to an appropriate
allocation granularity,
or **NULL**, the size has to be aligned to an appropriate allocation granularity.

If the **pmem2_vm_reservation_new**() succeeds in creating a reservation, it instantiates a new
**struct pmem2_vm_reservation** object describing the reservation. The pointer to this object
is stored in the user-provided variable via the *rsv_ptr* pointer. If the function
fails, an appropriate error value will be returned. For a list of possible return
values please see [RETURN VALUE](#return-value)

After instantiating an object via the **pmem2_vm_reservation_new**() function, it
may be disposed of using the **pmem2_vm_reservation_delete**() function.

The **pmem2_vm_reservation_delete**() function destroys the object describing
the reservation and unmaps virtual memory region
the *struct pmem2_vm_reservation** had assigned during the initialization.
For the delete function to succeed, it is required that the reservation passed via
the *rsv_ptr* pointer does not contain any mappings.

# RETURN VALUE #

The **pmem2_vm_reservation_new**() and **pmem2_vm_reservation_delete**() functions return 0
on success or a negative error code on failure.

The function returns 0 on success or a negative error code on failure.

# ERRORS #

The **pmem2_vm_reservation_new**() function can fail with the following errors:

* **PMEM2_E_ADDRESS_UNALIGNED** - argument *addr* is not aligned to the appropriate
allocation granularity.

* **PMEM2_E_MAPPING_EXISTS** - mapping already exists in the range (*addr*, *addr* + *size*).

It can also return **-EAGAIN**, **-ENOMEM** from the underlying **mmap**(2) function and **-ENOMEM**
in case of insufficient memory to allocate an instance of *struct pmem2_vm_reservation*.

The **pmem2_vm_reservation_delete**() function can fail with the following errors:

* **PMEM2_E_RESERVATION_NOT_EMPTY** - reservation contains mappings.

It can also return errors from the underlying **munmap**(2) function.

# ERRORS #

The **pmem2_vm_reservation_new**() and **pmem2_vm_reservation_delete**() functions are not supported yet.

# SEE ALSO #

**pmem2_config_set_vm_reservation**(3), **libpmem2**(7) and **<http://pmem.io>**

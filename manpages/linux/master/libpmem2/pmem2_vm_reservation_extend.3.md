---
layout: manual
Content-Style: 'text/css'
title: PMEM2_VM_RESERVATION_EXTEND
collection: libpmem2
header: PMDK
date: pmem2 API version 1.0
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2021, Intel Corporation)

[comment]: <> (pmem2_vm_reservation_extend.3 -- man page for libpmem2 pmem2_vm_reservation_extend operation)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[ERRORS](#errors)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmem2_vm_reservation_extend**() - extends existing virtual memory reservation

# SYNOPSIS #

```c
#include <libpmem2.h>

struct pmem2_vm_reservation;
int pmem2_vm_reservation_extend(struct pmem2_vm_reservation *rsv, size_t size);
```

# DESCRIPTION #

The **pmem2_vm_reservation_extend**() function extends an existing virtual memory
reservation by the given *size*. For the function to succeed the size has to be aligned
to an appropraite allocation granularity.

If the **pmem2_vm_reservation_extend**() succeeds in extending a reservation, it provides
placeholder virtual memory range that starts from an address at the end of the old reservation.
Mappings made to the extended reservation are preserved.

If the function fails, reservation will be left as it was and appropriate error value will be returned.

# RETURN VALUE #

The **pmem2_vm_reservation_extend**() function returns 0 on success or a negative error code on failure.

# ERRORS #

The **pmem2_vm_reservation_extend**() function can fail with the following errors:

* **PMEM2_E_MAPPING_EXISTS** - the range that the reservation would be extended by is already occupied
by existing mapping.

It can also return **-EAGAIN**, **-ENOMEM** from the underlying **mmap**(2) function.

# SEE ALSO #

**pmem2_config_set_vm_reservation**(3), **libpmem2**(7) and **<http://pmem.io>**

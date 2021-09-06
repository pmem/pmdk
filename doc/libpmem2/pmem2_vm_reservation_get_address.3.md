---
layout: manual
Content-Style: 'text/css'
title: _MP(PMEM2_VM_RESERVATION_GET_ADDRESS, 3)
collection: libpmem2
header: PMDK
date: pmem2 API version 1.0
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2020, Intel Corporation)

[comment]: <> (pmem2_vm_reservation_get_address.3 -- man page for libpmem2 pmem2_vm_reservation_get_address operation)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmem2_vm_reservation_get_address**() - reads address of the virtual memory reservation

# SYNOPSIS #

```c
#include <libpmem2.h>

void *pmem2_vm_reservation_get_address(struct pmem2_vm_reservation *rsv);
```

# DESCRIPTION #

The **pmem2_vm_reservation_get_address**() function reads address of the created virtual memory
reservation. The *rsv* parameter points to the structure describing the reservation created using
the **pmem2_vm_reservation_new**(3) function.

# RETURN VALUE #

The **pmem2_vm_reservation_get_address**() function returns a pointer to the virtual memory
reservation area.

# SEE ALSO #

**pmem2_vm_reservation_new**(3), **libpmem2**(7) and **<https://pmem.io>**

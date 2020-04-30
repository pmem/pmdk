---
layout: manual
Content-Style: 'text/css'
title: _MP(PMEM2_VM_RESERVATION_NEW, 3)
collection: libpmem2
header: PMDK
date: pmem2 API version 1.9
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2020, Intel Corporation)

[comment]: <> (pmem2_vm_reservation_new.3 -- man page for libpmem2 virtual memory reservation API)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmem2_vm_reservation_new**(), **pmem2_vm_reservation_delete**() - creates or deletes virtual memory
reservation that is made based on the pmem2_vm_reservation structure

# SYNOPSIS #

```c
#include <libpmem2.h>

struct pmem2_vm_reservation;
void pmem2_vm_reservation_new(struct pmem2_vm_reservation **rsv,
		size_t size, void *address);
void pmem2_vm_reservation_delete(struct pmem2_vm_reservation **rsv);
```

# DESCRIPTION #

The **pmem2_vm_reservation_new**() function ... .
The **pmem2_vm_reservation_delete**() function ... .

# RETURN VALUE #

**pmem2_vm_reservation_new**() returns ... .
**pmem2_vm_reservation_delete**() returns ... .

# SEE ALSO #

**libpmem2**(7), **pmem2_config_set_vm_reservation**(3) and **<http://pmem.io>**

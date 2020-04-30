---
layout: manual
Content-Style: 'text/css'
title: _MP(PMEM2_CONFIG_SET_VM_RESERVATION, 3)
collection: libpmem2
header: PMDK
date: pmem2 API version 1.0
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2020, Intel Corporation)

[comment]: <> (pmem2_config_set_vm_reservation.3 -- man page for libpmem2 config API)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmem2_config_set_vm_reservation**() - sets the pmem2_vm_reservation structure basing on the
values in the pmem2_config structure

# SYNOPSIS #

```c
#include <libpmem2.h>

struct pmem2_config;
struct pmem2_vm_reservation;
int pmem2_config_set_vm_reservation(struct pmem2_config *cfg,
		struct pmem2_vm_reservation *rsv, size_t offset);
```

# DESCRIPTION #

The **pmem2_config_set_vm_reservation**() function is not supported yet.

# RETURN VALUE #

**pmem2_config_set_vm_reservation**() returns PMEM2_E_NOSUPP .

# SEE ALSO #

**libpmem2**(7), **pmem2_vm_reservation_new**(3) and **<http://pmem.io>**

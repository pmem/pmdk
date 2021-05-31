---
layout: manual
Content-Style: 'text/css'
title: PMEMSET_CONFIG_SET_RESERVATION
collection: libpmemset
header: PMDK
date: pmemset API version 1.0
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2021, Intel Corporation)

[comment]: <> (pmemset_config_set_reservation.3 -- man page for pmemset_config_set_reservation)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmemset_config_set_reservation**() - sets a reservation for entire pmemset to use

# SYNOPSIS #

```c
#include <libpmemset.h>

struct pmemset_config;
void pmemset_config_set_reservation(struct pmemset_config *config,
		struct pmem2_vm_reservation *rsv);
```

# DESCRIPTION #

The **pmemset_config_set_reservation**() function sets a virtual memory reservation for
entire pmemset object.

Setting the reservation in pmemset configuration limits the future part mappings to the virtual
address space spanned by the provided reservation. Every part mapping is made to the provided
reservation, instead of the virtual address space of the process calling **pmemset_part_map**(3).
For more information about part mapping behavior and virtual memory reservation please see
**pmemset_part_map**(3) and **pmem2_vm_reservation_new**(3), respectively.

# RETURN VALUE

The **pmemset_config_set_reservation**() does not return any value.

# SEE ALSO #

**pmem2_vm_reservation_new**(3), **pmemset_new**(3),
**pmemset_part_map**(3), **libpmemset**(7) and **<http://pmem.io>**

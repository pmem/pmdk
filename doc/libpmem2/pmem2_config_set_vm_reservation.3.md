---
draft: false
slider_enable: true
description: ""
disclaimer: "The contents of this web site and the associated <a href=\"https://github.com/pmem\">GitHub repositories</a> are BSD-licensed open source."
aliases: ["pmem2_config_set_vm_reservation.3.html"]
title: "libpmem2 | PMDK"
header: "pmem2 API version 1.0"
---

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
int pmem2_config_set_vm_reservation(struct pmem2_config *config,
		struct pmem2_vm_reservation *rsv, size_t rsv_offset);
```

# DESCRIPTION #

The **pmem2_config_set_vm_reservation**() function sets the virtual memory reservation
and an offset to be used during a mapping. *rsv* should be already initialized. Please see
**pmem2_vm_reservation_new**(3) for details. *rsv_offset* marks the offset in the
reservation for the mapping.

# RETURN VALUE #

**pmem2_config_set_vm_reservation**() function always returns 0.

# SEE ALSO #

**pmem2_vm_reservation_new**(3), **libpmem2**(7) and **<https://pmem.io>**

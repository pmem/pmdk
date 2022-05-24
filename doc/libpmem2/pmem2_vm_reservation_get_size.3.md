---
draft: false
slider_enable: true
description: ""
disclaimer: "The contents of this web site and the associated <a href=\"https://github.com/pmem\">GitHub repositories</a> are BSD-licensed open source."
aliases: ["pmem2_vm_reservation_get_size.3.html"]
title: "libpmem2 | PMDK"
header: "pmem2 API version 1.0"
---

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2020, Intel Corporation)

[comment]: <> (pmem2_vm_reservation_get_size.3 -- man page for libpmem2 virtual memory reservation operation)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmem2_vm_reservation_get_size**() - reads virtual memory reservation size

# SYNOPSIS #

```c
#include <libpmem2.h>

size_t pmem2_vm_reservation_get_size(struct pmem2_vm_reservation *rsv);
```

# DESCRIPTION #

The **pmem2_vm_reservation_get_size**() function reads size of the created virtual memory
reservation. The *rsv* parameter points to the structure describing virtual memory reservation
created using the **pmem2_vm_reservation_new**(3) function.

# RETURN VALUE #

The **pmem2_vm_reservation_get_size**() function returns a size of the virtual reservation area.

# SEE ALSO #

**pmem2_vm_reservation_new**(3), **libpmem2**(7) and **<https://pmem.io>**

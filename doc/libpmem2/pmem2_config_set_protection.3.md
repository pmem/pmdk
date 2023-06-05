---
draft: false
slider_enable: true
description: ""
disclaimer: "The contents of this web site and the associated <a href=\"https://github.com/pmem\">GitHub repositories</a> are BSD-licensed open source."
aliases: ["pmem2_config_set_protection.3.html"]
title: "libpmem2 | PMDK"
header: "pmem2 API version 1.0"
---

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2020-2023, Intel Corporation)

[comment]: <> (pmem2_config_set_protection.3 -- man page for libpmem2 config API)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[ERRORS](#errors)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmem2_config_set_protection**() - set a protection flags in pmem2_config structure.

# SYNOPSIS #

```c
#include <libpmem2.h>

struct pmem2_config;

#define PMEM2_PROT_EXEC		(1U << 29)
#define PMEM2_PROT_READ		(1U << 30)
#define PMEM2_PROT_WRITE	(1U << 31)
#define PMEM2_PROT_NONE		0

int pmem2_config_set_protection(struct pmem2_config *cfg,
		unsigned prot);
```

# DESCRIPTION #

The **pmem2_config_set_protection**() function sets the protection flags
which will be used for memory mapping. The default value
in pmem2_config structure is **PMEM2_PROT_READ | PMEM2_PROT_WRITE**.
The *\prot* argument describes the desired memory protection of the mapping.
The memory protection cannot conflict with the file opening-mode.
*\*config* should be already initialized,
please see **pmem2_config_new**(3) for details.

It is either PROT_NONE or the bitwise OR of one or more of the following flags:

* **PMEM2_PROT_EXEC** - Pages may be executed.

* **PMEM2_PROT_READ** - Pages may be read.

* **PMEM2_PROT_WRITE** - Pages may be written.

* **PMEM2_PROT_NONE** - Pages may not be accessed.

# RETURN VALUE #

The **pmem2_config_set_protection**() function returns 0 on success
or a negative error code on failure.

# ERRORS #

The **pmem2_config_set_protection**() can fail with the following errors:

* **PMEM2_E_INVALID_PROT_FLAG** - some or all of the provided flags are not valid.

# SEE ALSO #

**libpmem2**(7), **pmem2_config_new**(3), **pmem2_map_new**(3)
and **<https://pmem.io>**

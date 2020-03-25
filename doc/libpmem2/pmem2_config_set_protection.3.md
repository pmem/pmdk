---
layout: manual
Content-Style: 'text/css'
title: _MP(PMEM2_CONFIG_SET_PROTECTION, 3)
collection: libpmem2
header: PMDK
date: pmem2 API version 1.0
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2020, Intel Corporation)

[comment]: <> (pmem2_config_set_protection.3 -- man page for libpmem2 config API)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmem2_config_set_protection**() - set the requested protection flags in pmem2_config structure

# SYNOPSIS #

```c
#include <libpmem2.h>

struct pmem2_config;
enum pmem2_protection_flag {
	PMEM2_PROT_EXEC = (1U << 29),
	PMEM2_PROT_READ = (1U << 30),
	PMEM2_PROT_WRITE = (1U << 31),
};
int pmem2_config_set_protection(struct pmem2_config *cfg,
		enum pmem2_protection_flag prot);
```

# DESCRIPTION #

The **pmem2_config_set_protection**() function sets the protection flags which will be used
for memory mapping. The *\prot* argument describes the desired memory protection of the mapping
(the requested mapping protection cannot conflict with the file opening-mode).
*\*config* should be already initialized, please see **pmem2_config_new**(3) for details.

Available protection flags:

* **PMEM2_PROT_MAX** - Default value. When this flag is set, libmem2 tries to do mapping
with READ and WRITE permissions. If it doesn't succeed it tries only READ mode.

* **PMEM2_PROT_EXEC** - Pages may be executed.

* **PMEM2_PROT_READ** - Pages may be read.

* **PMEM2_PROT_WRITE** - Pages may be written.

# RETURN VALUE #

When **pmem2_config_set_protection**() succeeds it returns 0. Otherwise, it returns the following
error value:

* **PMEM2_E_INVALID_PROT_FLAG** - some or all of the provided flags are not valid.

# SEE ALSO #

**libpmem2**(7), **pmem2_config_new**(3), **pmem2_map**(3)
and **<http://pmem.io>**

---
layout: manual
Content-Style: 'text/css'
title: _MP(PMEM2_CONFIG_SET_SHARING, 3)
collection: libpmem2
header: PMDK
date: pmem2 API version 1.0
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2020, Intel Corporation)

[comment]: <> (pmem2_config_set_sharing.3 -- man page for libpmem2 config API)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmem2_config_set_sharing**() - set sharing in the pmem2_config structure

# SYNOPSIS #

```c
#include <libpmem2.h>

struct pmem2_config;
int pmem2_config_set_sharing(struct pmem2_config *config, unsigned sharing);
```

# DESCRIPTION #

The **pmem2_config_set_sharing**() function configures the sharing which will
determine the way **pmem2_map** will map the file. Sharing field takes two
possible options: **PMEM2_SHARED** and **PMEM2_PRIVATE**. Dafault value of
sharing in *\*config* is **PMEM2_SHARED**. *\*config* should be already
initialized, please see **pmem2_config_new**(3) for details.

Possible sharing values are:

* **PMEM2_SHARED** - All changes in the file mapping are visible to other mappings
of the same file and are transferred to the base file.

* **PMEM2_PRIVATE** - Create a private mapping. All changes in the file mapping
are not visible for other mappings of the same file. Any changes are not
transferred to the base file.

# RETURN VALUE #

The **pmem2_config_set_sharing**() function returns 0 on success. Otherwise, it
returns:

* **PMEM2_E_INVALID_FLAG** - invalid value, a flag differs than **PMEM2_SHARED**
and **PMEM2_PRIVATE**

# SEE ALSO #

**libpmem2**(7), **pmem2_config_new**(3), **pmem2_map**(3), **sysconf**(3)
and **<http://pmem.io>**

---
layout: manual
Content-Style: 'text/css'
title: _MP(PMEM2_CONFIG_SET_OFFSET, 3)
collection: libpmem2
header: PMDK
date: pmem2 API version 1.0
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2019, Intel Corporation)

[comment]: <> (pmem2_config_set_offset.3 -- man page for libpmem2 config API)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmem2_config_set_offset**() - set offset in the pmem2_config structure

# SYNOPSIS #

```c
#include <libpmem2.h>

struct pmem2_config;
int pmem2_config_set_offset(struct pmem2_config *config, size_t offset);
```

# DESCRIPTION #

The **pmem2_config_set_offset**() function configures the offset which will be used
to map the contents from the specified location of the source. *\*config* should be
already initialized, please see **pmem2_config_new**(3) for details. The *\offset*
must be a multiple of the alignment required for the config. To retrieve the alignment
required for specific *\*config* please use **pmem2_config_get_alignment**(3).

# RETURN VALUE #

The **pmem2_config_set_offset**() function returns 0 on success. Otherwise, it returns:

* **PMEM2_E_OFFSET_OUT_OF_RANGE** - argument out of range, offset is greater than
**INT64_MAX**

# SEE ALSO #

**libpmem2**(7), **pmem2_config_get_alignment**(3), **pmem2_config_new**(3),
**pmem2_map**(3), **sysconf**(3) and **<http://pmem.io>**

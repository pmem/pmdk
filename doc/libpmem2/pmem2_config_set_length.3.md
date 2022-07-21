---
draft: false
slider_enable: true
description: ""
disclaimer: "The contents of this web site and the associated <a href=\"https://github.com/pmem\">GitHub repositories</a> are BSD-licensed open source."
aliases: ["pmem2_config_set_length.3.html"]
title: "libpmem2 | PMDK"
header: "pmem2 API version 1.0"
---

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2019, Intel Corporation)

[comment]: <> (pmem2_config_set_length.3 -- man page for libpmem2 config API)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmem2_config_set_length**() - set length in the pmem2_config structure

# SYNOPSIS #

```c
#include <libpmem2.h>

struct pmem2_config;
int pmem2_config_set_length(struct pmem2_config *config, size_t length);
```

# DESCRIPTION #

The **pmem2_config_set_length**() function configures the length which will be used
for mapping. *\*config* should be already initialized, please see **pmem2_config_new**(3)
for details. The *\length* must be a multiple of the alignment required for the data
source which will be used for mapping alongside the config.
To retrieve the alignment required for specific instance of *pmem2_source** use
**pmem2_source_alignment**(3).
By default, the length is equal to the size of the file that is being mapped.

# RETURN VALUE #

The **pmem2_config_set_length**() function always returns 0.

# SEE ALSO #

**libpmem2**(7), **pmem2_map_new**(3), **pmem2_source_alignment**(3),
**pmem2_config_new**(3), **sysconf**(3) and **<https://pmem.io>**

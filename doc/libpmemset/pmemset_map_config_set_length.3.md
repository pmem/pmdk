---
draft: false
slider_enable: true
description: ""
disclaimer: "The contents of this web site and the associated <a href=\"https://github.com/pmem\">GitHub repositories</a> are BSD-licensed open source."
aliases: ["pmemset_map_config_set_length.3.html"]
title: "libpmemset | PMDK"
header: "pmemset API version 1.0"
---

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2021, Intel Corporation)

[comment]: <> (pmemset_map_config_set_length.3 -- man page for pmemset_map_config_set_length)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmemset_map_config_set_length**() - set length of mapping in the map config.

# SYNOPSIS #

```c
#include <libpmemset.h>

void pmemset_config_set_length(struct pmemset_map_config *map_cfg,
	size_t length);
```

# DESCRIPTION #

The **pmemset_config_set_length**() function configures the length which will be used
for mapping. *\*map_cfg* should be already initialized, please see **pmemset_map_config_new**(3)
for details. The *\length* must be a multiple of the alignment required for the data
source which will be used for mapping alongside the config.
By default, the length is equal to the size of the file that is being mapped.

# RETURN VALUE

The **pmemset_map_config_set_length**() function returns no value.

# SEE ALSO #

**pmemset_map**(3), **pmemset_map_config_new**(3), **libpmemset**(7) and **<http://pmem.io>**

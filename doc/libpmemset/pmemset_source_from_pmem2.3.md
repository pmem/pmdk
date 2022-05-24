---
draft: false
slider_enable: true
description: ""
disclaimer: "The contents of this web site and the associated <a href=\"https://github.com/pmem\">GitHub repositories</a> are BSD-licensed open source."
aliases: ["pmemset_source_from_pmem2.3.html"]
title: "libpmemset | PMDK"
header: "pmemset API version 1.0"
---

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2020, Intel Corporation)

[comment]: <> (pmemset_source_from_pmem2.3 -- man page for pmemset_source_from_pmem2 function)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[ERRORS](#errors)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmemset_source_from_pmem2**() - creates an instance of persistent set
data source

# SYNOPSIS #

```c
#include <libpmemset.h>

int pmemset_source_from_pmem2(struct pmemset_source **src, struct pmem2_source *pmem2_src);
```

# DESCRIPTION #

The **pmemset_source_from_pmem2**() function instantiates a new *struct pmemset_source*
object describing set of data. The *pmemset_source* is created using *pmem2_source* from
**libpmem2**(7) library. For more details see **pmem2_source**(3) man page.

# RETURN VALUE #

The **pmemset_source_from_pmem2**() function returns 0 on success or a negative error code on failure.

# ERRORS #

**pmemset_source_from_pmem2**() can fail with the following errors:

- **-ENOMEM** - in case of insufficient memory to allocate an instance of *struct pmemset_source*.
- **PMEMSET_E_INVALID_PMEM2_SOURCE** - if the *struct pmem2_source* is NULL.

# SEE ALSO #

**errno**(3), **pmem2_source**(3),
**libpmem2**(7), **libpmemset**(7) and **<http://pmem.io>**

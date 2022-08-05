---
draft: false
slider_enable: true
description: ""
disclaimer: "The contents of this web site and the associated <a href=\"https://github.com/pmem\">GitHub repositories</a> are BSD-licensed open source."
aliases: ["pmem2_source_from_existing.3.html"]
title: "libpmem2 | PMDK"
header: "pmem2 API version 1.0"
---

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2022, Intel Corporation)

[comment]: <> (pmem2_source_from_existing.3 -- man page for pmem2_source_from_existing

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[ERRORS](#errors)<br />
[CAVEATS](#caveats)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmem2_source_from_existing**() - creates data source backed by an existing virtual
memory mapping

# SYNOPSIS #

```c
#include <libpmem2.h>

struct pmem2_source;

int pmem2_source_from_existing(struct pmem2_source **src, void *addr,
		size_t size, int is_pmem);
```

# DESCRIPTION #

The **pmem2_source_from_existing**() function instantiates a new *struct pmem2_source*
object describing an existing, mapped source.

The *addr* and *size* arguments for the function are the address and size of the
existing mapping. The *is_pmem* should be either *0* or *1* to indicate whether the
mapped file was resident on the PMEM device. **libpmem2**(7) leverages this
information to optimize persisting the changes made to the mappings created from
this source.

# RETURN VALUE #

The **pmem2_source_from_existing**() function returns 0 on success
or a negative error code on failure.

# ERRORS #

The **pmem2_source_from_existing**() can fail with the following errors:

* **-ENOMEM** - in case of insufficient
memory to allocate an instance of *struct pmem2_source*.

# SEE ALSO #
**errno**(3),
**libpmem2**(7) and **<https://pmem.io>**

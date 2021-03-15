---
layout: manual
Content-Style: 'text/css'
title: PMEM2_SOURCE_NUMA_NODE
collection: libpmem2
header: PMDK
date: pmem2 API version 1.0
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2020, Intel Corporation)

[comment]: <> (pmem2_source_numa_node.3 -- man page for pmem2_source_numa_node)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[ERRORS](#errors)<br />
[CAVEATS](#caveats)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmem2_source_numa_node**() - returns data source numa node

# SYNOPSIS #

```c
#include <libpmem2.h>

struct pmem2_source;
int pmem2_source_numa_node(const struct pmem2_source *source, int *numa_node);
```

# DESCRIPTION #

The **pmem2_source_numa_node**() function retrieves the numa node of the given data source.
The numa node can be used to, e.g., pin threads to near-memory cores. The numa node is stored in *\*numa_node*.
It is the same value that is shown as `numa_node` in `ndctl list -v`.

# RETURN VALUE #

The **pmem2_source_numa_node**() function returns 0 on success.
If the function fails, the *\*numa_node* variable is left unmodified and a negative error code is returned.

# ERRORS #

The **pmem2_source_numa_node**() can fail with the following errors:

On all systems:

* **PMEM2_E_NOSUPP** - source type or operating system not supported (see #caveats for details.)

on Linux:

* **PMEM2_E_DAX_REGION_NOT_FOUND** - no **ndctl_region** could be determined for the source.

* **PMEM2_E_INVALID_FILE_TYPE** - if the source points to a directory.

* -**errno** set by failing **ndctl_new**, while trying to create a new context.

# CAVEATS #

This call requires **libndctl** to retrieve the numa information.
It only works for sources that are actually located on persistent memory, i.e., devdax or fsdax.
As anonymous sources are not backed by files on persistent memory, this method is not supported for them.
It also does not work under Windows or systems without **libndctl**.

# SEE ALSO #

**errno**(3), **ndctl_new**(3), **pmem2_source_from_handle**(3),
**pmem2_source_from_fd**(3), **libpmem2**(7), **libndctl**(7) and **<https://pmem.io>**

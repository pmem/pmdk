---
draft: false
slider_enable: true
description: ""
disclaimer: "The contents of this web site and the associated <a href=\"https://github.com/pmem\">GitHub repositories</a> are BSD-licensed open source."
aliases: ["pmempool_sync.3.html"]
title: "libpmempool | PMDK"
header: "pmempool API version 1.3"
---

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2017-2022, Intel Corporation)

[comment]: <> (pmempool_sync.3 -- man page for pmempool sync and transform)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[ERRORS](#errors)<br />
[NOTES](#notes)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmempool_sync**(), **pmempool_transform**() - pool set synchronization and transformation

# SYNOPSIS #

```c
#include <libpmempool.h>

int pmempool_sync(const char *poolset_file, unsigned flags); (EXPERIMENTAL)
int pmempool_transform(const char *poolset_file_src,
	const char *poolset_file_dst, unsigned flags); (EXPERIMENTAL)
```

# DESCRIPTION #

The **pmempool_sync**() function synchronizes data between replicas within
a pool set.

**pmempool_sync**() accepts two arguments:

* *poolset_file* - a path to a pool set file,

* *flags* - a combination of flags (ORed) which modify how synchronization
is performed.

>NOTE: Only the pool set file used to create the pool should be used
for syncing the pool.

>NOTE: The **pmempool_sync**() cannot do anything useful if there
are no replicas in the pool set.  In such case, it fails with an error.

>NOTE: Replication is only supported for **libpmemobj**(7) pools.

The following flags are available:

* **PMEMPOOL_SYNC_DRY_RUN** - do not apply changes, only check for viability of
synchronization.

**pmempool_sync**() checks that the metadata of all replicas in
a pool set is consistent, i.e. all parts are healthy, and if any of them is
not, the corrupted or missing parts are recreated and filled with data from
one of the healthy replicas.

If a pool set has the option *SINGLEHDR* (see **poolset**(5)),
the internal metadata of each replica is limited to the beginning of the first
part in the replica. If the option *NOHDRS* is used, replicas contain no
internal metadata. In both cases, only the missing parts or the ones which
cannot be opened are recreated with the **pmempool_sync**() function.

**pmempool_transform**() modifies the internal structure of a pool set.
It supports the following operations:

* adding one or more replicas,

* removing one or more replicas,

* adding or removing pool set options.

Only one of the above operations can be performed at a time.

**pmempool_transform**() accepts three arguments:

* *poolset_file_src* - pathname of the pool *set* file for the source
pool set to be changed,

* *poolset_file_dst* - pathname of the pool *set* file that defines the new
structure of the pool set,

* *flags* - a combination of flags (ORed) which modify how synchronization
is performed.

The following flags are available:

* **PMEMPOOL_TRANSFORM_DRY_RUN** - do not apply changes, only check for viability of
transformation.

>NOTE: The *transform* operation is only supported for **libpmemobj**(7) pools.

# RETURN VALUE #

**pmempool_sync**() and **pmempool_transform**() return 0 on success.
Otherwise, they return -1 and set *errno* appropriately.

# ERRORS #

**EINVAL** Invalid format of the input/output pool set file.

**EINVAL** Unsupported *flags* value.

**EINVAL** There is only master replica defined in the input pool set passed
  to **pmempool_sync**().

**EINVAL** The source pool set passed to **pmempool_transform**() is not a
  **libpmemobj** pool.

**EINVAL** The input and output pool sets passed to **pmempool_transform**()
  are identical.

**EINVAL** Attempt to perform more than one transform operation at a time.

# NOTES #

The **pmempool_sync**() API is experimental and it may change in future
versions of the library.

The **pmempool_transform**() API is experimental and it may change in future
versions of the library.

# SEE ALSO #

**libpmemobj**(7) and **<https://pmem.io>**

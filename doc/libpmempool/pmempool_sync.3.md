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

_UW(pmempool_sync), _UW(pmempool_transform) - pool set synchronization and transformation

# SYNOPSIS #

```c
#include <libpmempool.h>

_UWFUNCR1(int, pmempool_sync, *poolset_file,=q=
	unsigned flags=e=, =q= (EXPERIMENTAL)=e=)
_UWFUNCR12(int, pmempool_transform, *poolset_file_src,
	*poolset_file_dst, unsigned flags, =q= (EXPERIMENTAL)=e=)
```

_UNICODE()

# DESCRIPTION #

The _UW(pmempool_sync) function synchronizes data between replicas within
a pool set.

_UW(pmempool_sync) accepts two arguments:

* *poolset_file* - a path to a pool set file,

* *flags* - a combination of flags (ORed) which modify how synchronization
is performed.

>NOTE: Only the pool set file used to create the pool should be used
for syncing the pool.

>NOTE: The _UW(pmempool_sync) cannot do anything useful if there
are no replicas in the pool set.  In such case, it fails with an error.

>NOTE: Replication is only supported for **libpmemobj**(7) pools.

The following flags are available:

* **PMEMPOOL_SYNC_DRY_RUN** - do not apply changes, only check for viability of
synchronization.

_UW(pmempool_sync) checks that the metadata of all replicas in
a pool set is consistent, i.e. all parts are healthy, and if any of them is
not, the corrupted or missing parts are recreated and filled with data from
one of the healthy replicas.

_WINUX(,=q=If a pool set has the option *SINGLEHDR* (see **poolset**(5)),
the internal metadata of each replica is limited to the beginning of the first
part in the replica. If the option *NOHDRS* is used, replicas contain no
internal metadata. In both cases, only the missing parts or the ones which
cannot be opened are recreated with the _UW(pmempool_sync) function.=e=)

_UW(pmempool_transform) modifies the internal structure of a pool set.
It supports the following operations:

* adding one or more replicas,

* removing one or more replicas _WINUX(.,=q=,

* adding or removing pool set options.=e=)

Only one of the above operations can be performed at a time.

_UW(pmempool_transform) accepts three arguments:

* *poolset_file_src* - pathname of the pool *set* file for the source
pool set to be changed,

* *poolset_file_dst* - pathname of the pool *set* file that defines the new
structure of the pool set,

* *flags* - a combination of flags (ORed) which modify how synchronization
is performed.

The following flags are available:

* **PMEMPOOL_TRANSFORM_DRY_RUN** - do not apply changes, only check for viability of
transformation.

_WINUX(=q=When adding or deleting replicas, the two pool set files can differ only in the
definitions of replicas which are to be added or deleted. One cannot add and
remove replicas in the same step. Only one of these operations can be performed
at a time. Reordering replicas is not supported.
Also, to add a replica it is necessary for its effective size to match or
exceed the pool size. Otherwise the whole operation fails and no changes are
applied. The effective size of a replica is the sum of sizes of all its part
files decreased by 4096 bytes per each part file. The 4096 bytes of each part
file is utilized for storing internal metadata of the pool part files.=e=)

>NOTE: The *transform* operation is only supported for **libpmemobj**(7) pools.

# RETURN VALUE #

_UW(pmempool_sync) and _UW(pmempool_transform) return 0 on success.
Otherwise, they return -1 and set *errno* appropriately.

# ERRORS #

**EINVAL** Invalid format of the input/output pool set file.

**EINVAL** Unsupported *flags* value.

**EINVAL** There is only master replica defined in the input pool set passed
  to _UW(pmempool_sync).

**EINVAL** The source pool set passed to _UW(pmempool_transform) is not a
  **libpmemobj** pool.

**EINVAL** The input and output pool sets passed to _UW(pmempool_transform)
  are identical.

**EINVAL** Attempt to perform more than one transform operation at a time.

# NOTES #

The _UW(pmempool_sync) API is experimental and it may change in future
versions of the library.

The _UW(pmempool_transform) API is experimental and it may change in future
versions of the library.

# SEE ALSO #

**libpmemobj**(7) and **<https://pmem.io>**

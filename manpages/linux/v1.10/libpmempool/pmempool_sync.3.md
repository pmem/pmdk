---
layout: manual
Content-Style: 'text/css'
title: PMEMPOOL_SYNC
collection: libpmempool
header: PMDK
date: pmempool API version 1.3
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2017-2018, Intel Corporation)

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

int pmempool_sync(const char *poolset_file, 
	unsigned flags); (EXPERIMENTAL)
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

>NOTE: At the moment, replication is only supported for **libpmemobj**(7)
pools, so **pmempool_sync**() cannot be used with other pool types
(**libpmemlog**(7), **libpmemblk**(7)).

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

* removing one or more replicas ,

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



When adding or deleting replicas, the two pool set files can differ
only in the definitions of replicas which are to be added or deleted. When
adding or removing pool set options (see **poolset**(5)), the rest of both pool
set files have to be of the same structure. The operation of adding/removing
a pool set option can be performed on a pool set with local replicas only. To
add/remove a pool set option to/from a pool set with remote replicas, one has
to remove the remote replicas first, then add/remove the option, and finally
recreate the remote replicas having added/removed the pool set option to/from
the remote replicas' poolset files.
To add a replica it is necessary for its effective size to match or exceed the
pool size. Otherwise the whole operation fails and no changes are applied.
If none of the pool set options is used, the effective size of a replica is the
sum of sizes of all its part files decreased by 4096 bytes per each part file.
The 4096 bytes of each part file is utilized for storing internal metadata of
the pool part files.
If the option *SINGLEHDR* is used, the effective size of a replica is the sum of
sizes of all its part files decreased once by 4096 bytes. In this case only
the first part contains internal metadata.
If the option *NOHDRS* is used, the effective size of a replica is the sum of
sizes of all its part files. In this case none of the parts contains internal
metadata.

>NOTE: At the moment, *transform* operation is only supported for
**libpmemobj**(7) pools, so **pmempool_transform**() cannot be used with other
pool types (**libpmemlog**(7), **libpmemblk**(7)).

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

**ENOTSUP** The pool set contains a remote replica, but remote replication
  is not supported (**librpmem**(7) is not available).

# NOTES #

The **pmempool_sync**() API is experimental and it may change in future
versions of the library.

The **pmempool_transform**() API is experimental and it may change in future
versions of the library.

# SEE ALSO #

**libpmemlog**(7), **libpmemobj**(7) and **<https://pmem.io>**

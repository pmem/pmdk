---
layout: manual
Content-Style: 'text/css'
title: PMEMPOOL_SYNC
collection: libpmempool
header: PMDK
date: pmempool API version 1.3
...

[comment]: <> (Copyright 2017-2018, Intel Corporation)

[comment]: <> (Redistribution and use in source and binary forms, with or without)
[comment]: <> (modification, are permitted provided that the following conditions)
[comment]: <> (are met:)
[comment]: <> (    * Redistributions of source code must retain the above copyright)
[comment]: <> (      notice, this list of conditions and the following disclaimer.)
[comment]: <> (    * Redistributions in binary form must reproduce the above copyright)
[comment]: <> (      notice, this list of conditions and the following disclaimer in)
[comment]: <> (      the documentation and/or other materials provided with the)
[comment]: <> (      distribution.)
[comment]: <> (    * Neither the name of the copyright holder nor the names of its)
[comment]: <> (      contributors may be used to endorse or promote products derived)
[comment]: <> (      from this software without specific prior written permission.)

[comment]: <> (THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS)
[comment]: <> ("AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT)
[comment]: <> (LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR)
[comment]: <> (A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT)
[comment]: <> (OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,)
[comment]: <> (SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT)
[comment]: <> (LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,)
[comment]: <> (DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY)
[comment]: <> (THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT)
[comment]: <> ((INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE)
[comment]: <> (OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.)

[comment]: <> (pmempool_sync.3 -- man page for pmempool sync and transform)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[ERRORS](#errors)<br />
[NOTES](#notes)<br />
[SEE ALSO](#see-also)<br />


# NAME #

**pmempool_syncU**()/**pmempool_syncW**(), **pmempool_transformU**()/**pmempool_transformW**() - pool set synchronization and transformation


# SYNOPSIS #

```c
#include <libpmempool.h>

int pmempool_syncU(const char *poolset_file, 
	unsigned flags); (EXPERIMENTAL)
int pmempool_syncW(const wchar_t *poolset_file, 
	unsigned flags); (EXPERIMENTAL)
int pmempool_transformU(const char *poolset_file_src,
	const char *poolset_file_dst, unsigned flags); (EXPERIMENTAL)
int pmempool_transformW(const wchar_t *poolset_file_src,
	const wchar_t *poolset_file_dst, unsigned flags); (EXPERIMENTAL)
```


>NOTE: The PMDK API supports UNICODE. If the **PMDK_UTF8_API** macro is
defined, basic API functions are expanded to the UTF-8 API with postfix *U*.
Otherwise they are expanded to the UNICODE API with postfix *W*.


# DESCRIPTION #

The **pmempool_syncU**()/**pmempool_syncW**() function synchronizes data between replicas within
a pool set.

**pmempool_syncU**()/**pmempool_syncW**() accepts two arguments:

* *poolset_file* - a path to a pool set file,

* *flags* - a combination of flags (ORed) which modify how synchronization
is performed.

>NOTE: Only the pool set file used to create the pool should be used
for syncing the pool.

>NOTE: The **pmempool_syncU**()/**pmempool_syncW**() cannot do anything useful if there
are no replicas in the pool set.  In such case, it fails with an error.

>NOTE: At the moment, replication is only supported for **libpmemobj**(7)
pools, so **pmempool_syncU**()/**pmempool_syncW**() cannot be used with other pool types
(**libpmemlog**(7), **libpmemblk**(7)).

The following flags are available:

* **PMEMPOOL_SYNC_DRY_RUN** - do not apply changes, only check for viability of
synchronization.

**pmempool_syncU**()/**pmempool_syncW**() checks that the metadata of all replicas in
a pool set is consistent, i.e. all parts are healthy, and if any of them is
not, the corrupted or missing parts are recreated and filled with data from
one of the healthy replicas.




**pmempool_transformU**()/**pmempool_transformW**() modifies the internal structure of a pool set.
It supports the following operations:

* adding one or more replicas,

* removing one or more replicas .

Only one of the above operations can be performed at a time.

**pmempool_transformU**()/**pmempool_transformW**() accepts three arguments:

* *poolset_file_src* - pathname of the pool *set* file for the source
pool set to be changed,

* *poolset_file_dst* - pathname of the pool *set* file that defines the new
structure of the pool set,

* *flags* - a combination of flags (ORed) which modify how synchronization
is performed.

The following flags are available:

* **PMEMPOOL_TRANSFORM_DRY_RUN** - do not apply changes, only check for viability of
transformation.

When adding or deleting replicas, the two pool set files can differ only in the
definitions of replicas which are to be added or deleted. One cannot add and
remove replicas in the same step. Only one of these operations can be performed
at a time. Reordering replicas is not supported.
Also, to add a replica it is necessary for its effective size to match or
exceed the pool size. Otherwise the whole operation fails and no changes are
applied. The effective size of a replica is the sum of sizes of all its part
files decreased by 4096 bytes per each part file. The 4096 bytes of each part
file is utilized for storing internal metadata of the pool part files.



>NOTE: At the moment, *transform* operation is only supported for
**libpmemobj**(7) pools, so **pmempool_transformU**()/**pmempool_transformW**() cannot be used with other
pool types (**libpmemlog**(7), **libpmemblk**(7)).


# RETURN VALUE #

**pmempool_syncU**()/**pmempool_syncW**() and **pmempool_transformU**()/**pmempool_transformW**() return 0 on success.
Otherwise, they return -1 and set *errno* appropriately.


# ERRORS #

**EINVAL** Invalid format of the input/output pool set file.

**EINVAL** Unsupported *flags* value.

**EINVAL** There is only master replica defined in the input pool set passed
  to **pmempool_syncU**()/**pmempool_syncW**().

**EINVAL** The source pool set passed to **pmempool_transformU**()/**pmempool_transformW**() is not a
  **libpmemobj** pool.

**EINVAL** The input and output pool sets passed to **pmempool_transformU**()/**pmempool_transformW**()
  are identical.

**EINVAL** Attempt to perform more than one transform operation at a time.

**ENOTSUP** The pool set contains a remote replica, but remote replication
  is not supported (**librpmem**(7) is not available).


# NOTES #

The **pmempool_syncU**()/**pmempool_syncW**() API is experimental and it may change in future
versions of the library.

The **pmempool_transformU**()/**pmempool_transformW**() API is experimental and it may change in future
versions of the library.


# SEE ALSO #

**libpmemlog**(7), **libpmemobj**(7) and **<http://pmem.io>**

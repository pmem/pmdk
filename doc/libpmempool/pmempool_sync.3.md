---
layout: manual
Content-Style: 'text/css'
title: _MP(PMEMPOOL_SYNC, 3)
collection: libpmempool
header: NVM Library
date: pmempool API version 1.1
...

[comment]: <> (Copyright 2017, Intel Corporation)

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
[NOTES](#notes)<br />
[SEE ALSO](#see-also)<br />


# NAME #

_UW(pmempool_sync), _UW(pmempool_transform) -- pool set synchronization and transformation


# SYNOPSIS #

```c
#include <libpmempool.h>

!ifdef{WIN32}
{
int pmempool_syncU(const char *poolset_file, unsigned flags); (EXPERIMENTAL)
int pmempool_syncW(const wchar_t *poolset_file, unsigned flags); (EXPERIMENTAL)
int pmempool_transformU(const char *poolset_file_src, (EXPERIMENTAL)
	const char *poolset_file_dst, unsigned flags);
int pmempool_transformW(const wchar_t *poolset_file_src, (EXPERIMENTAL)
	const wchar_t *poolset_file_dst, unsigned flags);
}{
int pmempool_sync(const char *poolset_file, unsigned flags); (EXPERIMENTAL)
int pmempool_transform(const char *poolset_file_src,
	const char *poolset_file_dst,
	unsigned flags); (EXPERIMENTAL)
}
```

_UNICODE()


# DESCRIPTION #

The _UW(pmempool_sync) function synchronizes data between replicas within
a pool set.

_UW(pmempool_sync) accepts two arguments:

* *poolset_file* - a path to a pool set file,

* *flags* - a combination of flags (ORed) which modify the way of
synchronization.

>NOTE: Only the pool set file used to create the pool should be used
for syncing the pool.

The following flags are available:

* **PMEMPOOL_DRY_RUN** - do not apply changes, only check for viability of
synchronization.

_UW(pmempool_sync) checks that the metadata of all replicas in
a pool set is consistent, i.e. all parts are healthy, and if any of them is
not, the corrupted or missing parts are recreated and filled with data from
one of the healthy replicas.

_UW(pmempool_transform) modifies the internal structure of a
pool set. It supports the following operations:

* adding one or more replicas,

* removing one or more replicas,

* reordering of replicas.


!pmempool_transform accepts three arguments:

* *poolset_file_src* - a path to a pool set file which defines the source
pool set to be changed,

* *poolset_file_dst* - a path to a pool set file which defines the target
structure of the pool set,

* *flags* - a combination of flags (ORed) which modify the way of
synchronization.

The following flags are available:

* **PMEMPOOL_DRY_RUN** - do not apply changes, only check for viability of
synchronization.

When adding or deleting replicas, the two pool set files can differ only in the
definitions of replicas which are to be added or deleted. One cannot add and
remove replicas in the same step. Only one of these operations can be performed
at a time. Reordering replicas can be combined with either of them.
Also, to add a replica it is necessary for its effective size to match or
exceed the pool size. Otherwise the whole operation fails and no changes are
applied. The effective size of a replica is the sum of sizes of all its part
files decreased by 4096 bytes per each part file. The 4096 bytes of each part
file is utilized for storing internal metadata of the pool part files.


# RETURN VALUE #

_UW(pmempool_sync) and _UW(pmempool_transform) return 0 on success.
Otherwise, they return -1 and set *errno* appropriately.


# NOTES #


The _UW(pmempool_sync) API is experimental and it may change in future
versions of the library.

The _UW(pmempool_transform) API is experimental and it may change in future
versions of the library.

# SEE ALSO #

**libpmemlog**(7), **libpmemobj**(7) and **<http://pmem.io>**

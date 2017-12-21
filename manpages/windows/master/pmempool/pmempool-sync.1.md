---
layout: manual
Content-Style: 'text/css'
title: PMEMPOOL-SYNC
collection: pmempool
header: PMDK
date: pmem Tools version 1.3
...

[comment]: <> (Copyright 2016-2017, Intel Corporation)

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

[comment]: <> (pmempool-sync.1 -- man page for pmempool-sync)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[EXAMPLES](#examples)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmempool-sync** -- Synchronize replicas or their parts within a pool set.


# SYNOPSIS #

```
pmempool sync [options] <poolset_file>
```

NOTE: Only the pool set file used to create the pool should be used
for syncing the pool.

# DESCRIPTION #

The **pmempool sync** command synchronizes data between replicas within
a pool set. It checks if metadata of all replicas in a pool set
are consistent, i.e. all parts are healthy, and if any of them is not,
the corrupted or missing parts are recreated and filled with data from one of
the healthy replicas.
Currently synchronizing data is allowed only for **pmemobj** pools (see
**libpmemobj**(7)).



##### Available options: #####

`-d, --dry-run`

: Enable dry run mode. In this mode no changes are applied, only check for
viability of synchronization.

`-v, --verbose`

: Increase verbosity level.

`-h, --help`

: Display help message and exit.


# SEE ALSO #

**pmempool(1)**, **libpmemblk(7)**, **libpmemlog(7)**,
**libpmempool(7)** and **<http://pmem.io>**

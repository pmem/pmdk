---
layout: manual
Content-Style: 'text/css'
title: pmempool-transform(1)
header: NVM Library
date: pmem Tools version 1.0.1
...

[comment]: <> (Copyright 2016, Intel Corporation)

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

[comment]: <> (pmempool-transform.1 -- man page for pmempool-transform)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[EXAMPLES](#examples)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmempool-transform** -- Modify internal structure of a poolset.

# SYNOPSIS #

```
pmempool transform [options] <poolset_file_src> <poolset_file_dst>
```

# DESCRIPTION #

The **pmempool transform** command modifies internal structure of a poolset
defined by the `poolset_file_src` file, according to a structure described in
the `poolset_file_dst` file.

The following operations are supported:

* adding replicas - a new replica can be added and synchronized with other
replicas in the poolset,

* removing replicas - replicas can be removed from the poolset.

Currently these operations are allowed only for **pmemobj** pools (see
**libpmemobj**(3)).


The *poolset_file_src* argument provides the source poolset to be changed.

The *poolset_file_dst* argument points to the target poolset.

When adding or deleting replica, the two poolset files can differ only in the
definitions of replicas which are to be added or deleted.
Also, to add a replica it is necessary for its size to match or exceed the
poolset size. Otherwise the whole operation fails and no changes are applied.


##### Available options: #####

`-d, --dry-run`

: Enable dry run mode. In this mode no changes are applied, only check for
viability of the operation is performed.

`-h, --help`

: Display help message and exit.


# EXAMPLES #

##### Example 1. #####

Let us assume that files `/poolset_file_src` and `/poolset_file_dst` have the
following contents, respectively:

```
PMEMPOOLSET
20M /0/partfile1
20M /0/partfile2
21M /0/partfile3
REPLICA
40M /1/partfile1
20M /1/partfile2
```

```
PMEMPOOLSET
40M /0/partfile4
21M /0/partfile3
REPLICA
30M /1/partfile3
30M /1/partfile4
REPLICA
50M /2/partfile1
20M /2/partfile2

```
Then, the command

`pmempool transform /poolset_file_src /poolset_file_dst`

yields the following:

* The first two parts of the master replica are joined into one large part.
The third part of the replica remains unchanged.

* The first regular replica's structure will change from two parts of size
40M and 20M to two parts of size 30M and 30M.

* The second regular replica consisting of two parts will be created.
The size of the new replica is 70M, but the size of the pool remains 60M.

##### Example 2. #####

Now, let `/poolset_file_src` and `/poolset_file_dst` have the
following contents, respectively:

```
PMEMPOOLSET
20M /0/partfile1
50M /0/partfile2
REPLICA
40M /1/partfile1
20M /1/partfile2
```

```
PMEMPOOLSET
20M /0/partfile1
50M /0/partfile2

```
Then

`pmempool_transform "/poolset_file_src" "/poolset_file_dst");`

deletes the second replica from the poolset. The second replica remains
unchanged and the size of the pool remains(!) 60M.

# SEE ALSO #

**libpmemblk(3)**, **libpmemlog(3)**, **pmempool(1)**

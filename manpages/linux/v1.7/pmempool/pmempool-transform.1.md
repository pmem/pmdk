---
layout: manual
Content-Style: 'text/css'
title: PMEMPOOL-TRANSFORM
collection: pmempool
header: PMDK
date: pmem Tools version 1.4
...

[comment]: <> (Copyright 2016-2018, Intel Corporation)

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

**pmempool-transform** - Modify internal structure of a pool set.


# SYNOPSIS #

```
pmempool transform [options] <poolset_file_src> <poolset_file_dst>
```


# DESCRIPTION #

The **pmempool transform** command modifies internal structure of a pool set
defined by the `poolset_file_src` file, according to a structure described in
the `poolset_file_dst` file.

The following operations are supported:

* adding replicas - one or more new replicas can be added and synchronized with
other replicas in the pool set,

* removing replicas - one or more replicas can be removed from the pool set
,

* adding or removing pool set options.

Only one of the above operations can be performed at a time.

Currently adding and removing replicas are allowed only for **pmemobj** pools
(see **libpmemobj**(7)).


The *poolset_file_src* argument provides the source pool set to be changed.

The *poolset_file_dst* argument points to the target pool set.



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
If none of the poolset options is used, the effective size of a replica is the
sum of sizes of all its part files decreased by 4096 bytes per each part file.
The 4096 bytes of each part file is utilized for storing internal metadata of
the pool part files.
If the option *SINGLEHDR* is used, the effective size of a replica is the sum of
sizes of all its part files decreased once by 4096 bytes. In this case only
the first part contains internal metadata.
If the option *NOHDRS* is used, the effective size of a replica is the sum of
sizes of all its part files. In this case none of the parts contains internal
metadata.


##### Available options: #####

`-d, --dry-run`

: Enable dry run mode. In this mode no changes are applied, only check for
viability of the operation is performed.

`-v, --verbose`

: Increase verbosity level.

`-h, --help`

: Display help message and exit.


# EXAMPLES #

##### Example 1. #####

Let files `/path/poolset_file_src` and `/path/poolset_file_dst` have the
following contents:

```
PMEMPOOLSET
20M /0/partfile1
20M /0/partfile2
25M /0/partfile3
REPLICA
40M /1/partfile1
20M /1/partfile2
```

```
PMEMPOOLSET
20M /0/partfile1
20M /0/partfile2
25M /0/partfile3
REPLICA
40M /1/partfile1
20M /1/partfile2
REPLICA
50M /2/partfile1
20M /2/partfile2

```
Then, the command

`pmempool transform /path/poolset_file_src /path/poolset_file_dst`

adds a replica to the pool set. All other replicas remain unchanged and
the size of the pool remains 60M.

##### Example 2. #####

Let files `/path/poolset_file_src` and `/path/poolset_file_dst` have the
following contents:

```
PMEMPOOLSET
20M /0/partfile1
20M /0/partfile2
25M /0/partfile3
REPLICA
40M /1/partfile1
20M /1/partfile2
```

```
PMEMPOOLSET
20M /0/partfile1
20M /0/partfile2
25M /0/partfile3
```
Then

`pmempool_transform /path/poolset_file_src /path/poolset_file_dst`

deletes the second replica from the pool set. The first replica remains
unchanged and the size of the pool is still 60M.


# SEE ALSO #

**pmempool(1)**, **libpmemblk(7)**, **libpmemlog(7)**,
**libpmempool(7)** and **<http://pmem.io>**

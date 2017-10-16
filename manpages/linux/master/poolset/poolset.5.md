---
layout: manual
Content-Style: 'text/css'
title: POOLSET
collection: poolset
header: NVM Library
date: poolset API version 1.0
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

[comment]: <> (poolset.5 -- man page that describes format of pool set file)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[SEE ALSO](#see-also)<br />


# NAME #

poolset -- persistent memory pool configuration file format


# SYNOPSIS #

```c
mypool.set
```


# DESCRIPTION #

Depending on the configuration of the system, the available space of non-volatile
memory space may be divided into multiple memory devices. In such case, the
maximum size of the transactional object store could be limited by the capacity
of a single memory device.

The **libpmemobj**(7), **libpmemblk** and **libpmemlog** allows building object
stores spanning multiple memory devices by creation of persistent memory pools
consisting of multiple files, where each part of such a *pool set* may be
stored on different pmem-aware filesystem.

In case of **libpmemobj**(7) to improve reliability and eliminate the single point of
failure, all the changes of the data stored in the persistent memory pool could be also
automatically written to local or remote pool replicas, thereby providing
a backup for a persistent memory pool by producing a *mirrored pool set*. In practice,
the pool replicas may be considered as binary copies of the "master" pool set.
Data replication is not supported in **libpmemblk**(7) and **libpmemlog**(7).

The set file for each type of pool is a plain text file, which must start
with the line containing a *PMEMPOOLSET* string, followed by the specification
of all the pool parts in the next lines.
For each part, the file size and the absolute path must be provided.

The size has to be compliant with the format specified in IEC 80000-13, IEEE 1541
or the Metric Interchange Format. Standards accept SI units with obligatory
B - kB, MB, GB, ... (multiplier by 1000) and IEC units with optional "iB"
- KiB, MiB, GiB, ..., K, M, G, ... - (multiplier by 1024).

The path of a part can point to a Device DAX and in such case the size
argument can be set to an "AUTO" string, which means that the size of the device
will be automatically resolved at pool creation time.
When using Device DAX there's also one additional restriction - it is not allowed
to concatenate more than one Device DAX device in a single pool set
if the configured internal alignment is other than 4KiB.  In such case a pool set
can consist only of a single part (single Device DAX).
Please see **ndctl-create-namespace**(1) for information on how to configure
desired alignment on Device DAX.

Device DAX is the device-centric analogue of Filesystem DAX. It allows memory
ranges to be allocated and mapped without need of an intervening file system.
For more information please see **ndctl-create-namespace**(1).

The minimum file size of each part of the pool set is defined in **\<libpmemblk.h\>**
as **PMEMBLK_MIN_PART** for block pool, in **\<libpmemobj.h\>** as **PMEMOBJ_MIN_PART**
for obj pool or in **\<libpmemlog.h>** as **PMEMLOG_MIN_PART** for log pool.
The net pool size of the pool set is equal to:

```
net_pool_size = sum_over_all_parts(page_aligned_part_size - 4KiB) + 4KiB
```
where
```
page_aligned_part_size = part_size & ~(page_size - 1)
```

Note that page size is OS specific. For more information please see **sysconf**(3).
The minimum net pool size of a pool set allowed by the library for a block pool
is defined in **\<libpmemblk.h\>** as **PMEMBLK_MIN_POOL**, for log pool in
**\<libpmemlog.h\>** as **PMEMLOG_MIN_POOL** and in **\<libpmemobj.h\>** as **PMEMOBJ_MIN_POOL**
for obj pool. Lines starting with "#" character are ignored.

Here is the example "myblkpool.set" file:

```
PMEMPOOLSET
100G /mountpoint0/myfile.part0
200G /mountpoint1/myfile.part1
400G /mountpoint2/myfile.part2
```

The files in the set may be created by running one of the following commands:

```
$ pmempool create blk <bsize> myblkpool.set
```
for block pool or for log pool:
```
$ pmempool create log <bsize> mylogpool.set
```

>WARNING:
Creation of all the parts of the pool set and the associated replica sets can be done
with the **pmemobj_create**(3), **pmemblk_create**(3), **pmemlog_create**(3) function
or by using the **pmempool**(1) utility.

Restoring data from a local or remote replica can be done by using the
**pmempool-sync**(1) command or **pmempool_sync**() API from the
**libpmempool**(3) library.

Modifications of a pool set file configuration can be done by using the
**pmempool-transform**(1) command or **pmempool_transform**() API from the
**libpmempool**(3) library.

When creating the pool set consisting of multiple files, or when creating
the replicated pool set, the *path* argument passed to **pmemobj_create**(3),
**pmemblk_create**(3), **pmemlog_create**(3) must point to the special *set*
file that defines the pool layout and the location of all the parts of the pool set.

When opening the pool set consisting of multiple files, or when opening the replicated
pool set, the *path* argument passed to **pmemobj_create**(3), **pmemblk_create**(3),
**pmemlog_create**(3) must not point to the pmemobj, pmemblk or pmemlog memory pool
file, but to the same *set* file that was used for the pool set creation.
If an error prevents any of the pool set files from being opened, or if the actual
size of any file does not match the corresponding part size defined in *set* file
**pmemobj_create**(3), **pmemblk_create**(3) or **pmemlog_create**(3) return
NULL and set *errno* appropriately.

Sections defining the replica sets are optional. There could be multiple replica
sections and each must start with the line containing a *REPLICA* string.
Lines starting with "#" character are ignored. A replica can be local
or remote. In case of a local replica, the REPLICA line has to consist of the *REPLICA*
string only and it has to be followed by at least one line defining a part of
the local replica. The format of such line is the same as the format of the line
defining a part of the pool as described above.

In case of a remote replica, the *REPLICA* keyword has to be followed by
an address of a remote host (in the format recognized by the **ssh**(1)
remote login client) and a relative path to a remote pool set file (located
in the root config directory on the target node - see **librpmem**(3)):

```
REPLICA [<user>@]<hostname> [<relative-path>/]<remote-pool-set-file>
```

There are no other lines in the remote replica section - the REPLICA line
defines a remote replica entirely. Here is the example of "myobjpool.set"
file:

```
PMEMPOOLSET
100G /mountpoint0/myfile.part0
200G /mountpoint1/myfile.part1
400G /mountpoint2/myfile.part2

# local replica
REPLICA
500G /mountpoint3/mymirror.part0
200G /mountpoint4/mymirror.part1

# remote replica
REPLICA user@example.com remote-objpool.set
```
The files in the obj set may be created by running the following command:

```
$ pmempool create --layout="mylayout" obj myobjpool.set
```

# SEE ALSO #

**ndctl-create-namespace**(1), **pmemblk_create**(3), **pmemlog_create**(3),
**pmemobj_create**(3), **sysconf**(3), **libpmemblk**(7), **libpmemlog**(7),
**libpmemobj**(7) and **<http://pmem.io>**

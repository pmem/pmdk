---
layout: manual
Content-Style: 'text/css'
title: POOLSET!5
header: NVM Library
date: libpmem API version 1.0
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

[comment]: <> (poolset.3 -- man page that describes format of pool set file)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[SEE ALSO](#see-also)<br />


# NAME #

poolset -- file in specific format that describes pools set and parts.


# SYNOPSIS #

```c
mypool.set
```


# DESCRIPTION #

The set file is a plain text file, which must start with the line containing
a *PMEMPOOLSET* string, followed by the specification of all the pool parts
in the next lines. For each part, the file size and the absolute path must be provided.

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
as **PMEMBLK_MIN_PART** for block pool or in **\<libpmemlob.h>** as **PMEMLOG_MIN_PART** for log pool.
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
is defined in **\<libpmemblk.h\>** as **PMEMBLK_MIN_POOL** and for log pool in
**\<libpmemlog.h\>** as **PMEMLOG_MIN_POOL**
Lines starting with "#" character are ignored.

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

# SEE ALSO #

**ndctl-create-namespace**(1), **sysconf**(3),
**libpmemblk**(7) and **<http://pmem.io>**

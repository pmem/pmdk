---
draft: false
slider_enable: true
description: ""
disclaimer: "The contents of this web site and the associated <a href=\"https://github.com/pmem\">GitHub repositories</a> are BSD-licensed open source."
aliases: ["poolset.5.html"]
title: "poolset | PMDK"
header: "poolset API version 1.0"
---

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2017-2022, Intel Corporation)

[comment]: <> (poolset.5 -- man page that describes format of pool set file)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[REPLICAS](#replicas)<br />
[POOL SET OPTIONS](#pool-set-options)<br />
[NOTES](#notes)<br />
[SEE ALSO](#see-also)<br />

# NAME #

poolset - persistent memory pool configuration file format

# SYNOPSIS #

```c
mypool.set
```

# DESCRIPTION #

Depending on the configuration of the system, the available non-volatile
memory space may be divided into multiple memory devices. In such case, the
maximum size of the transactional object store could be limited by the capacity
of a single memory device. Therefore, **libpmemobj**(7) allow building object
stores spanning multiple memory devices by creation of persistent memory pools
consisting of multiple files, where each part of such a *pool set* may be stored
on a different pmem-aware filesystem.

To improve reliability and eliminate single point of failure, **libpmemobj**(7)
also allows all the data written to a persistent memory pool to be copied
to local pool *replicas*, thereby providing backup for the
persistent memory pool by producing a *mirrored pool set*. In practice,
the pool replicas may be considered as binary copies of the "master" pool set.

The *set* file for each type of pool is a plain text file. Lines in the file
are formatted as follows:

+ The first line of the file must be the literal string "PMEMPOOLSET"

+ The pool parts are specified, one per line, in the format:

	*size* *pathname*

+ *Replica* sections, if any, start with the literal string "REPLICA".
See **REPLICAS**, below, for further details.

+ Pool set options, if any, start with literal string *OPTION*.
See **POOL SET OPTIONS** below for details.

+ Lines starting with "#" are considered comments and are ignored.

The *size* must be compliant with the format specified in IEC 80000-13, IEEE 1541
or the Metric Interchange Format. These standards accept SI units with
obligatory B - kB, MB, GB, ... (multiplier by 1000) suffixes, and IEC units
with optional "iB" - KiB, MiB, GiB, ..., K, M, G, ... - (multiplier by 1024)
suffixes.

*pathname* must be an absolute pathname.

The *pathname* of a part can point to a Device DAX. Device DAX is the
device-centric analogue of Filesystem DAX. It allows memory ranges to be
allocated and mapped without need of an intervening file system.

Pools created on Device DAX have additional options and restrictions:

+ The *size* may be set to "AUTO", in which case the size of the device will be
automatically resolved at pool creation time.

+ To concatenate more than one Device DAX device into a single pool set, the
configured internal alignment of the devices must be 4KiB, unless the
*SINGLEHDR* or *NOHDRS* option is used in the pool set file.
See **POOL SET OPTIONS** below for details.

Please see **ndctl-create-namespace**(1) for more information on Device DAX,
including how to configure desired alignment.

The minimum file size of each part of the pool set
is defined as **PMEMOBJ_MIN_PART** in **\<libpmemobj.h\>**

The net pool size of the pool set is equal to:

```
net_pool_size = sum_over_all_parts(page_aligned_part_size - 4KiB) + 4KiB
```
where
```
page_aligned_part_size = part_size & ~(page_size - 1)
```

Note that page size is OS specific. For more information please see
**sysconf**(3).

The minimum net pool size of a pool set
is defined as **PMEMOBJ_MIN_POOL** in **\<libpmemobj.h\>**

Here is an example "mypool.set" file:

```
PMEMPOOLSET
OPTION NOHDRS
100G /mountpoint0/myfile.part0
200G /mountpoint1/myfile.part1
400G /mountpoint2/myfile.part2
```

The files in the set may be created by running one of the following commands.
To create a obj pool:

```
$ pmempool create obj mypool.set
```

# REPLICAS #

Sections defining replica sets are optional. There may be multiple replica
sections.

Replica sections begin with a line containing only the literal string
"REPLICA", followed by one or more pool part lines as described above.

Here is an example "myobjpool.set" file with replicas:

```
PMEMPOOLSET
100G /mountpoint0/myfile.part0
200G /mountpoint1/myfile.part1
400G /mountpoint2/myfile.part2

# local replica
REPLICA
500G /mountpoint3/mymirror.part0
200G /mountpoint4/mymirror.part1
```

The files in the object pool set may be created by running the following command:

```
$ pmempool create --layout="mylayout" obj myobjpool.set
```

# POOL SET OPTIONS #

Pool set options can appear anywhere after the line with *PMEMPOOLSET* string.
Pool set file can contain several pool set options. The following options are
supported:

+ *SINGLEHDR*

If the *SINGLEHDR* option is used, only the first part in each replica contains
the pool part internal metadata. In that case the effective size of a replica
is the sum of sizes of all its part files decreased once by 4096 bytes.

When using the *SINGLEHDR* option, one can concatenate more than one
Device DAX devices with any internal alignments in one replica.

Using the *SINGLEHDR* option has important implications for data
integrity checking and recoverability in case of a pool set damage.
See _UW(pmempool_sync) API for more information about pool set recovery.

# DIRECTORIES #

Providing a directory as a part's *pathname* allows the pool to dynamically
create files and consequently removes the user-imposed limit on the size
of the pool.

The *size* argument of a part in a directory poolset becomes the size of the
address space reservation required for the pool. In other words, the size
argument is the maximum theoretical size of the mapping. This value can be
freely increased between instances of the application, but decreasing it below
the real required space will result in an error when attempting to open the
pool.

The directory must NOT contain user created files with extension *.pmem*,
otherwise the behavior is undefined. If a file created by the library within
the directory is in any way altered (resized, renamed) the behavior is
undefined.

A directory poolset must exclusively use directories to specify paths -
combining files and directories will result in an error. A single replica can
consist of one or more directories. If there are multiple directories, the
address space reservation is equal to the sum of the sizes.

The order in which the files are created is unspecified, but the library will
try to maintain equal usage of the directories.

By default pools grow in 128 megabyte increments.

Only poolsets with the *SINGLEHDR* option can safely use directories.

# NOTES #

Creation of all the parts of the pool set and the associated replica sets can
be done with the **pmemobj_create**(3) function, or by using the **pmempool**(1) utility.

Restoring data from a replica can be done by using the
**pmempool-sync**(1) command or the _UW(pmempool_sync) API from the
**libpmempool**(7) library.

Modifications of a pool set file configuration can be done by using the
**pmempool-transform**(1) command or the _UW(pmempool_transform) API from the
**libpmempool**(7) library.

When creating a pool set consisting of multiple files, or when creating
a replicated pool set, the *path* argument passed to **pmemobj_create**(3)
must point to the special *set* file that defines the pool layout and the location
of all the parts of the pool set.

When opening a pool set consisting of multiple files, or when opening a
replicated pool set, the *path* argument passed to **pmemobj_open**(3)
must point to the same *set* file that was used for pool set creation.

# SEE ALSO #

**ndctl-create-namespace**(1), **pmemobj_create**(3), **sysconf**(3),
**libpmemobj**(7) and **<https://pmem.io>**

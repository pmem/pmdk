---
draft: false
slider_enable: true
description: ""
disclaimer: "The contents of this web site and the associated <a href=\"https://github.com/pmem\">GitHub repositories</a> are BSD-licensed open source."
aliases: ["pmempool-transform.1.html"]
title: "pmempool | PMDK"
header: "pmem Tools version 1.4"
---

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2016-2023, Intel Corporation)

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

# NOTE #

> NOTICE:
The **libpmemblk** library is deprecated since PMDK 1.13.0 release.

# DESCRIPTION #

The **pmempool transform** command modifies internal structure of a pool set
defined by the `poolset_file_src` file, according to a structure described in
the `poolset_file_dst` file.

The following operations are supported:

* adding replicas - one or more new replicas can be added and synchronized with
other replicas in the pool set,

* removing replicas - one or more replicas can be removed from the pool set
_WINUX(.,=q=,

* adding or removing pool set options.=e=)

Only one of the above operations can be performed at a time.

Currently adding and removing replicas are allowed only for **pmemobj** pools
(see **libpmemobj**(7)).

The *poolset_file_src* argument provides the source pool set to be changed.

The *poolset_file_dst* argument points to the target pool set.

_WINUX(=q=When adding or deleting replicas, the two pool set files can differ only in the
definitions of replicas which are to be added or deleted. One cannot add and
remove replicas in the same step. Only one of these operations can be performed
at a time. Reordering replicas is not supported
Also, to add a replica it is necessary for its effective size to match or exceed
the pool size. Otherwise the whole operation fails and no changes are applied.
Effective size of a replica is the sum of sizes of all its part files decreased
by 4096 bytes per each part file. The 4096 bytes of each part file is
utilized for storing internal metadata of the pool part files.=e=)

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
**libpmempool(7)** and **<https://pmem.io>**

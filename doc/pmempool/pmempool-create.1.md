---
draft: false
slider_enable: true
description: ""
disclaimer: "The contents of this web site and the associated <a href=\"https://github.com/pmem\">GitHub repositories</a> are BSD-licensed open source."
aliases: ["pmempool-create.1.html"]
title: "pmempool | PMDK"
header: "pmem Tools version 1.4"
---

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2016-2019, Intel Corporation)

[comment]: <> (pmempool-create.1 -- man page for pmempool-create)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[EXAMPLE](#example)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmempool-create** - create a persistent memory pool

# SYNOPSIS #

```
$ pmempool create [<options>] [<type>] [<bsize>] <file>
```

# DESCRIPTION #

The **pmempool** invoked with *create* command creates a pool file
of specified type. Depending on a pool type it is possible to provide more properties of pool.

Valid pool types are: **blk**, **log** and **obj** which stands for
*pmemblk*, *pmemlog* and *pmemobj* pools respectively. By default
the pool file is created with *minimum* allowed size for specified
pool type. The minimum sizes for **blk**, **log** and **obj** pool
types are **PMEMBLK_MIN_POOL**, **PMEMLOG_MIN_POOL** and **PMEMOBJ_MIN_POOL**
respectively. See **libpmemblk**(7), **libpmemlog**(7)
and **libpmemobj**(7) for details.

For *pmemblk* pool type block size *\<bsize\>* is a required argument.

In order to set custom size of pool use **-s** option, or use **-M** option
to create a pool of maximum available size on underlying file system.

The *size* argument may be passed in format that permits only the upper-case
character for byte - B as specified in IEC 80000-13, IEEE 1541 and the
Metric Interchange Format. Standards accept SI units with obligatory
B - kB, MB, GB, ... which means multiplier by 1000 and IEC units with
optional "iB" - KiB, MiB, GiB, ..., K, M, G, ... - which means multiplier by 1024.

##### Available options: #####

`-s, --size <size>`

Size of pool file.

`-M, --max-size`

Set size of pool to available space of underlying file system.

`-m, --mode <octal>`

Set permissions to <octal> (the default is 0664) when creating the files.
If the file already exist the permissions are not changed.

`-i, --inherit <file>`

Create a new pool of the same size and other properties as *\<file\>*.

`-b, --clear-bad-blocks`

Clear bad blocks in existing files.

`-f, --force`

Remove the pool before creating.

`-v, --verbose`

Increase verbosity level.

`-h, --help`

Display help message and exit.

##### Options for PMEMBLK: #####

By default when creating a pmem **blk** pool, the **BTT** layout is *not*
written until the first *write operation* of block entry is performed.
Using **-w** option you can force writing the **BTT** layout by writing
zero data to specified block number. By default the *write operation*
is performed to block number 0. Please refer to **libpmemblk**(7) for details.

`-w, --write-layout`

Force writing the **BTT** layout by performing *write operation* to block number zero.

##### Options for PMEMOBJ: #####

By default when creating a pmem **obj** pool, the layout name provided to
the **libpmemobj** library is an empty string. Please refer to
**libpmemobj**(7) for details.

`-l, --layout <layout>`

Layout name of the **pmemobj** pool.

# EXAMPLE #

```
$ pmempool create blk 512 pool.blk
```

Create a blk pool file of minimum allowed size and block size 512 bytes

```
$ pmempool create log -M pool.log
```

Create a log pool file of maximum allowed size

```
$ pmempool create blk --size=4G --write-layout 1K pool.blk
```

Create a blk pool file of size 4G, block size 1K and write the BTT layout

```
$ pmempool create --layout my_layout obj pool.obj
```

Create an obj pool file of minimum allowed size and layout "my_layout"

```
$ pmempool create --inherit=pool.log new_pool.log
```

Create a pool file based on pool.log file

# SEE ALSO #

**pmempool**(1), **libpmemblk**(7), **libpmemlog**(7),
**libpmemobj**(7) and **<https://pmem.io>**

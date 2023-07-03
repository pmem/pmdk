---
draft: false
slider_enable: true
description: ""
disclaimer: "The contents of this web site and the associated <a href=\"https://github.com/pmem\">GitHub repositories</a> are BSD-licensed open source."
aliases: ["pmempool-create.1.html"]
title: "pmempool | PMDK"
header: "pmem Tools version 1.5"
---

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2016-2023, Intel Corporation)

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
$ pmempool create [<options>] [<type>] <file>
```

# DESCRIPTION #

The **pmempool** invoked with *create* command creates a pool file
of specified type.

The only valid pool types is: **obj** which stands for *pmemobj* pool.
By default the pool file is created with *minimum* allowed size for specified
pool type. The minimum sizes for **obj** pool type is **PMEMOBJ_MIN_POOL**.
See **libpmemobj**(7) for details.

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

`-l, --layout <layout>`

Layout name of the **pmemobj** pool. By default when creating a pmem **obj**
pool, the layout name provided to the **libpmemobj** library is an empty string.
Please refer to **libpmemobj**(7) for details.

# EXAMPLE #

```
$ pmempool create obj pool.obj
```
Create a obj pool file of minimum allowed size

```
$ pmempool create obj -M pool.obj
```

Create a obj pool file of maximum allowed size

```
$ pmempool create --layout my_layout obj pool.obj
```

Create an obj pool file of minimum allowed size and layout "my_layout"

```
$ pmempool create --inherit=pool.obj new_pool.obj
```

Create a pool file based on pool.obj file

# SEE ALSO #

**pmempool**(1), **libpmemobj**(7) and **<https://pmem.io>**

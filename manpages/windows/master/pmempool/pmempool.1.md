---
layout: manual
Content-Style: 'text/css'
title: PMEMPOOL
collection: pmempool
header: PMDK
date: pmem Tools version 1.4
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2016-2019, Intel Corporation)

[comment]: <> (pmempool.1 -- man page for pmempool)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[OPTIONS](#options)<br />
[COMMANDS](#commands)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmempool** - Persistent Memory Pool Management Tool

# SYNOPSIS #

```
$ pmempool [--help] [--version] <command> [<args>]
```

# DESCRIPTION #

The **pmempool** is a management tool for *Persistent Memory* pool files
created by **PMDK** libraries.

The main purpose of **pmempool** is to provide a user with a set of utilities
for off-line analysis and manipulation of pools created by pmem libraries.
The pmempool is a generic command which consists of subcommands for specific
purposes. Some of subcommands are required to work *without* any impact
on processed pool, but some of them *may* create a new or modify an existing one.

The **pmempool** may be useful for troubleshooting by system administrators
and for software developers who work on applications based on **PMDK**.
The latter may find these tools useful for testing and debugging purposes also.

# OPTIONS #

`-V, --version`

Prints the version of **pmempool**.

`-h, --help`

Prints synopsis and list of commands.

# COMMANDS #

Currently there is a following set of commands available:

+ **pmempool-info**(1) -
Prints information and statistics in human-readable format about specified pool.

+ **pmempool-check**(1) -
Checks pool's consistency and repairs pool if it is not consistent.

+ **pmempool-create**(1) -
Creates a pool of specified type with additional properties specific for this type of pool.

+ **pmempool-dump**(1) -
Dumps usable data from pool in hexadecimal or binary format.

+ **pmempool-rm**(1) -
Removes pool file or all pool files listed in pool set configuration file.

+ **pmempool-convert**(1) -
Updates the pool to the latest available layout version.

+ **pmempool-sync**(1) -
Synchronizes replicas within a poolset.

+ **pmempool-transform**(1) -
Modifies internal structure of a poolset.

+ **pmempool-feature**(1) -
Toggle or query a poolset features.

In order to get more information about specific *command* you can use **pmempool help <command>.**

# SEE ALSO #

**libpmemblk**(7), **libpmemlog**(7), **libpmemobj**(7)
and **<https://pmem.io>**

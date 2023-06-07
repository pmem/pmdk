---
draft: false
slider_enable: true
description: ""
disclaimer: "The contents of this web site and the associated <a href=\"https://github.com/pmem\">GitHub repositories</a> are BSD-licensed open source."
aliases: ["pmempool.1.html"]
title: "pmempool | PMDK"
header: "pmem Tools version 1.4"
---

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2016-2023, Intel Corporation)

[comment]: <> (pmempool.1 -- man page for pmempool)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[OPTIONS](#options)<br />
[COMMANDS](#commands)<br />
[DEBUGGING](#debugging)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmempool** - Persistent Memory Pool Management Tool

# SYNOPSIS #

```
$ pmempool [--help] [--version] <command> [<args>]
```

# NOTE #

> NOTICE:
The **libpmemblk** and **libpmemlog** libraries are deprecated since PMDK 1.13.0 release.

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

# DEBUGGING #

The debug logs are available only in the debug version of the tool,
which is not provided by binary packages, but can be built from sources.
The **pmempool.static-debug** binary blob can be found
in the 'src/tools/pmempool/' subdirectory.

+ **PMEMPOOL_TOOL_LOG_LEVEL**

The value of **PMEMPOOL_TOOL_LOG_LEVEL** enables trace points in the debug version
of the tool, as follows:

+ **0** - This is the default level when **PMEMPOOL_TOOL_LOG_LEVEL** is not set.
No log messages are emitted at this level.

+ **1** - Additional details on any errors detected are logged (in addition
to returning the *errno*-based errors as usual).

+ **2** - A trace of basic operations is logged.

+ **3** - Enables a very verbose amount of function call tracing in the tool.

+ **4** - Enables voluminous and fairly obscure tracing
information that is likely only useful to the **pmempool** developers.

Unless **PMEMPOOL_TOOL_LOG_FILE** is set, debugging output is written to *stderr*.

+ **PMEMPOOL_TOOL_LOG_FILE**

Specifies the name of a file where all logging information should be written.
If the last character in the name is "-", the *PID* of the current process
will be appended to the file name when the log file is created.
If **PMEMPOOL_TOOL_LOG_FILE** is not set, output is written to *stderr*.

# SEE ALSO #

**libpmemblk**(7), **libpmemlog**(7), **libpmemobj**(7)
and **<https://pmem.io>**

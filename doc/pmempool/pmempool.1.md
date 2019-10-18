---
layout: manual
Content-Style: 'text/css'
title: _MP(PMEMPOOL, 1)
collection: pmempool
header: PMDK
date: pmem Tools version 1.4
...

[comment]: <> (Copyright 2016-2019, Intel Corporation)

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
and **<http://pmem.io>**

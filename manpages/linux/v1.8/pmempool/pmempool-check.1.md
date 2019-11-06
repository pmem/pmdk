---
layout: manual
Content-Style: 'text/css'
title: PMEMPOOL-CHECK
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

[comment]: <> (pmempool-check.1 -- man page for pmempool-check)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[EXAMPLE](#example)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmempool-check** - check and repair persistent memory pool

# SYNOPSIS #

```
$ pmempool check [<options>] <file>
```

# DESCRIPTION #

The **pmempool** invoked with *check* command checks consistency of a given pool file.
If the pool file is consistent **pmempool** exits with 0 value. If the
pool file is not consistent non-zero error code is returned.

In case of any errors, the proper message is printed. The verbosity level
may be increased using **-v** option. The output messages may be also suppressed using
**-q** option.

It is possible to try to fix encountered problems using **-r** option.
In order to be sure this will not corrupt your data you can either create backup of the
pool file using **-b** option or just print what would be fixed
without modifying original pool using **-N** option.

> NOTE:
Currently, checking the consistency of a *pmemobj* pool is **not** supported.

##### Available options: #####

`-r, --repair`

Try to repair a pool file if possible.

`-y, --yes`

Answer yes on all questions.

`-d, --dry-run`

Don't execute, just show what would be done. Not supported on Device DAX.

`-N, --no-exec`

Deprecated alias for `dry-run`.

`-b, --backup <file>`

Create backup of a pool file before executing. Terminate if it is *not*
possible to create a backup file. This option requires **-r** option.

`-a, --advanced`

Perform advanced repairs. This option enables more aggressive steps in attempts
to repair a pool. This option requires `-r, --repair`.

`-q, --quiet`

Be quiet and don't print any messages.

`-v, --verbose`

Be more verbose.

`-h, --help`

Display help message and exit.

# EXAMPLE #

```
$ pmempool check pool.bin
```

Check consistency of "pool.bin" pool file

```
$ pmempool check --repair --backup pool.bin.backup pool.bin
```

Check consistency of "pool.bin" pool file, create backup and repair
if necessary.

```
$ pmempool check -rvN pool.bin
```

Check consistency of "pool.bin" pool file, print what would be repaired with
increased verbosity level.

# SEE ALSO #

**pmempool**(1), **libpmemblk**(7), **libpmemlog**(7),
**libpmemobj**(7), **libpmempool**(7) and **<http://pmem.io>**

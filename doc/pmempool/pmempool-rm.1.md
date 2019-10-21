---
layout: manual
Content-Style: 'text/css'
title: _MP(PMEMPOOL-RM, 1)
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

[comment]: <> (pmempool-rm.1 -- man page for pmempool-rm)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[EXAMPLE](#example)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmempool-rm** - remove a persistent memory pool

# SYNOPSIS #

```
$ pmempool rm [<options>] <file>..
```

# DESCRIPTION #

The **pmempool rm** command removes each specified file. If the specified file
is a pool set file, all pool files (single-file pool or part files) and remote
replicas are removed. By default the **pmempool rm** does not remove pool set
files. All local and remote pool files are removed using **unlink**(3) call,
except the pools created on **device dax** which are zeroed instead.
If specified file does not exist, the remote pool is broken or not accessible,
the **pmempool rm** command terminates with an error code. By default it prompts
before removing *write-protected* local files.
See **REMOTE REPLICATION** section for more details about support for remote
pools.
See **EXAMPLES** section for example usage of the *rm* command.

##### Available options: #####

`-h, --help`

Print help message

`-v, --verbose`

Be verbose and print all removing files.

`-s, --only-pools`

Remove only pool files and do not remove pool set files (default behaviour).

`-a, --all`

Remove all pool set files - local and remote.

`-l, --local`

Remove local pool set files.

`-r, --remote`

Remove remote pool set files.

`-f, --force`

Remove all specified files, ignore nonexistent files, never prompt.

`-i, --interactive`

Prompt before removing every single file or remote pool.

# REMOTE REPLICATION #

A remote pool is removed using **rpmem_remove**(3) function if **librpmem**(7)
library is available. If a pool set file contains remote replication but
**librpmem**(7) is not available, the **pmempool rm** command terminates with
an error code, unless the **-f, --force** option is specified.

# EXAMPLE #

```
$ pmempool rm pool.obj pool.blk
```

Remove specified pool files.

```
$ pmempool rm pool.set
```

Remove all pool files from the "pool.set", do not remove *pool.set* itself.

```
$ pmempool rm -a pool.set
```

Remove all pool files from the "pool.set", remove the local pool set file and all
remote pool set files.

# SEE ALSO #

**pmempool**(1), **libpmemblk**(7), **libpmemlog**(7),
**libpmemobj**(7), **librpmem**(7) and **<http://pmem.io>**

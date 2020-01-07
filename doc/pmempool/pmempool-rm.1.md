---
layout: manual
Content-Style: 'text/css'
title: _MP(PMEMPOOL-RM, 1)
collection: pmempool
header: PMDK
date: pmem Tools version 1.4
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2016-2018, Intel Corporation)

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
**libpmemobj**(7), **librpmem**(7) and **<https://pmem.io>**

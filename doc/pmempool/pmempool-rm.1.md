---
draft: false
slider_enable: true
description: ""
disclaimer: "The contents of this web site and the associated <a href=\"https://github.com/pmem\">GitHub repositories</a> are BSD-licensed open source."
aliases: ["pmempool-rm.1.html"]
title: "pmempool | PMDK"
header: "pmem Tools version 1.4"
---

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2016-2023, Intel Corporation)

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

# NOTE #

> NOTICE:
The **libpmemblk** and **libpmemlog** libraries are deprecated since PMDK 1.13.0 release.

# DESCRIPTION #

The **pmempool rm** command removes each specified file. If the specified file
is a pool set file, all pool files (single-file pool or part files) are removed.
By default the **pmempool rm** does not remove pool set
files. All pool files are removed using **unlink**(3) call,
except the pools created on **device dax** which are zeroed instead.
If specified file does not exist the **pmempool rm** command terminates with an error code.
By default it prompts before removing *write-protected* files.
See **EXAMPLES** section for example usage of the *rm* command.

##### Available options: #####

`-h, --help`

Print help message

`-v, --verbose`

Be verbose and print all removing files.

`-s, --only-pools`

Remove only pool files and do not remove pool set files (default behaviour).

`-a, --all`

Remove all pool set files.

`-l, --local`

Remove local pool set files.

`-f, --force`

Remove all specified files, ignore nonexistent files, never prompt.

`-i, --interactive`

Prompt before removing every single file.

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

Remove all pool files from the "pool.set".

# SEE ALSO #

**pmempool**(1), **libpmemlog**(7),
**libpmemobj**(7) and **<https://pmem.io>**

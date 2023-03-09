---
draft: false
slider_enable: true
description: ""
disclaimer: "The contents of this web site and the associated <a href=\"https://github.com/pmem\">GitHub repositories</a> are BSD-licensed open source."
aliases: ["pmempool-check.1.html"]
title: "pmempool | PMDK"
header: "pmem Tools version 1.4"
---

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2016-2023, Intel Corporation)

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

# NOTE #

> NOTICE:
The **libpmemblk** library is deprecated since PMDK 1.13.0 release.

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
**libpmemobj**(7), **libpmempool**(7) and **<https://pmem.io>**

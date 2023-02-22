---
draft: false
slider_enable: true
description: ""
disclaimer: "The contents of this web site and the associated <a href=\"https://github.com/pmem\">GitHub repositories</a> are BSD-licensed open source."
aliases: ["pmempool-sync.1.html"]
title: "pmempool | PMDK"
header: "pmem Tools version 1.4"
---

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2016-2023, Intel Corporation)

[comment]: <> (pmempool-sync.1 -- man page for pmempool-sync)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[EXAMPLES](#examples)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmempool-sync** - Synchronize replicas or their parts within a pool set.

# SYNOPSIS #

```
pmempool sync [options] <poolset_file>
```

NOTE: Only the pool set file used to create the pool should be used
for syncing the pool.

# NOTE #

> NOTICE:
The **libpmemblk** and **libpmemlog** libraries are deprecated since PMDK 1.13.0 release.

# DESCRIPTION #

The **pmempool sync** command synchronizes data between replicas within
a pool set. It checks if metadata of all replicas in a pool set
are consistent, i.e. all parts are healthy, and if any of them is not,
the corrupted or missing parts are recreated and filled with data from one of
the healthy replicas.
Currently synchronizing data is allowed only for **pmemobj** pools (see
**libpmemobj**(7)).

_WINUX(,=q=If a pool set has the option *SINGLEHDR* or *NOHDRS*
(see **poolset**(5)), **pmempool sync** command has limited capability
of checking its metadata. This is due to limited or no, respectively, internal
metadata at the beginning of pool set parts in every replica when either of the
options is used. In that cases, only missing parts or the ones which cannot
be opened are recreated.=e=)

##### Available options: #####

`-b, --bad-blocks`

: Fix bad blocks - it causes creating or reading special recovery files.
When bad blocks are detected, special recovery files have to be created
in order to fix them safely. A separate recovery file is created for each part
of the pool. The recovery files are created in the same directory
where the poolset file is located using the following name pattern:
\<poolset-file-name\> _r \<replica-number\> _p \<part-number\> _badblocks.txt
These recovery files are automatically removed if the sync operation finishes
successfully.

	If the last sync operation was interrupted and not finished correctly
(eg. the application crashed) and the bad blocks fixing procedure was
in progress, the bad block recovery files may be left over. In such case
bad blocks might have been cleared and zeroed, but the correct data from these
blocks was not recovered (not copied from a healthy replica), so the recovery
files MUST NOT be deleted manually, because it would cause a data loss.
Pmempool-sync should be run again with the '-b' option set. It will finish
the previously interrupted sync operation and copy correct data to zeroed
bad blocks using the left-over bad block recovery files (the bad blocks
will be read from the saved recovery files). Pmempool will delete the recovery
files automatically at the end of the sync operation.

	Using this option may have limitations depending on the operating system.
For details see description of the CHECK_BAD_BLOCKS feature
in **pmempool-feature**(1).

`-d, --dry-run`

: Enable dry run mode. In this mode no changes are applied, only check for
viability of synchronization.

`-v, --verbose`

: Increase verbosity level.

`-h, --help`

: Display help message and exit.

# SEE ALSO #

**pmempool(1)**, **libpmemblk(7)**, **libpmemlog(7)**,
**libpmempool(7)** and **<https://pmem.io>**

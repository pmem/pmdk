---
layout: manual
Content-Style: 'text/css'
title: pmempool-check(1)
...

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[EXAMPLES](#examples)<br />
[SEE ALSO](#see-also)<br />
[PMEMPOOL](#pmempool)<br />


### NAME ###

**pmempool-check**  − Check and repair Persistent Memory Pool

### SYNOPSIS ###

```
pmempool check [<options>] <file>
```

### DESCRIPTION ###

The **pmempool** invoked with `check` command checks consistency of a given pool file. If the pool file is consistent **pmempool** exits with 0 value. If the pool file is not consistent non-zero error code is returned.

In case of any errors, the proper message is printed. The verbosity level may be increased using `-v` option. The output messages may be also suppressed using `-q` option.

It is possible to try to fix encountered problems using `-r` option. In order to be sure this will not corrupt your data you can either create backup of the pool file using `-b` option or just print what would be fixed without modifying original pool using `-N` option.

##### Available options: #####

`-r, –repair`

: Try to repair a pool file if possible.

`-y, –yes`

: Answer yes on all questions.

`-N, –no-exec`

: Don’t execute, just show what would be done.

`-b, –backup <file>`

: Create backup of a pool file before executing. Terminate if it is *not* possible to create a backup file. This option requires `-r` option.

`-q, –quiet`

: Be quiet and don’t print any messages.

`-v, –verbose`

: Be more verbose.

`-h, –help`

: Display help message and exit.

### EXAMPLES ###

`pmempool check pool.bin`

: Check consistency of pool.bin pool file

`pmempool check –repair –backup pool.bin.backup pool.bin`

: Check consistency of pool.bin pool file, create backup and repair if necessary.

`pmempool check -rvN pool.bin`

: Check consistency of pool.bin pool file, print what would be repaired with increased verbosity level.

### SEE ALSO ###

**libpmemblk(3)**, **libpmemlog(3)**, **pmempool(1)**

### PMEMPOOL ###

Part of the **pmempool(1)** suite.

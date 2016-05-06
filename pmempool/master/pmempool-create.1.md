---
layout: manual
Content-Style: 'text/css'
title: pmempool-create(1)
...

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[EXAMPLES](#examples)<br />
[SEE ALSO](#see-also)<br />
[PMEMPOOL](#pmempool)<br />


### NAME ###

**pmempool-create** − Create Persistent Memory Pool of specified type

### SYNOPSIS ###

```
pmempool create [<options>] [<type>] [<bsize>] <file>
```

### DESCRIPTION ###

The **pmempool** invoked with `create` command creates a pool file of specified type. Depending on a pool type it is possible to provide more properties of pool.

Valid pool types are: **blk**, **log** and **obj** which stands for *pmemblk*, *pmemlog* and *pmemobj* pools respectively. By default the pool file is created with *minimum* allowed size for specified pool type. The minimum sizes for **blk**, **log** and **obj** pool types are **PMEMBLK_MIN_POOL**, **PMEMLOG_MIN_POOL** and **PMEMOBJ_MIN_POOL** respectively. See **libpmemblk**(3)**, libpmemlog**(3) and **libpmemobj**(3) for details.

For *pmemblk* pool type block size `<bsize>` is a required argument.

In order to set custom size of pool use `-s` option, or use `-M` option to create a pool of maximum available size on underlying file system.

The *size* argument may be passed in format that permits only the upper-case character for byte - B as specified in IEC 80000-13, IEEE 1541 and the Metric Interchange Format. Standards accept SI units with obligatory B - kB, MB, GB … which means multiplier by 1000 and IEC units with optional “iB” - KiB, MiB, GiB, …, K, M, G, … - which means multiplier by 1024.

##### Available options: #####

`-s, –size <size>`

: Size of pool file.

`-M, –max-size`

: Set size of pool to available space of underlying file system.

`-m, –mode <octal>`

: Set permissions to <octal> (the default is 0664).

`-i, –inherit <file>`

: Create a new pool of the same size and other properties as `<file>`.

`-v, –verbose`

: Increase verbosity level.

`-h, –help`

: Display help message and exit.


##### Options for PMEMBLK: #####

By default when creating a pmem **blk** pool, the **BTT** layout is *not* written until the first *write operation* of block entry is performed. Using `-w` option you can force writing **BTT** layout by writing zero data to specified block number. By default the *write operation* is performed to block number `0`. Please refer to **libpmemblk**(3) for details.

`-w, –write-layout`

: Force writing **BTT** layout by performing *write operation* to block number zero.

##### Options for PMEMOBJ: #####

By default when creating a pmem **obj** pool, the layout name provided to the **libpmemobj** library is an empty string. Please refer to **libpmemobj**(3) for details.

`-l, –layout <layout>`

: Layout name of the **pmemobj** pool.


### EXAMPLES ###

`pmempool create blk 512 pool.blk`

: Create a blk pool file of minimum allowed size and block size 512 bytes

`pmempool create log -M pool.log`

: Create a log pool file of maximum allowed size

`pmempool create blk –size=4G –write-layout 1K pool.blk`

: Create a blk pool file of size 4G, block size 1K and write BTT layout using write operation to the block number 1

`pmempool create –layout my_layout obj pool.obj`

: Create an obj pool file of minimum allowed size and layout ’my_layout’

`pmempool create –inherit=pool.log new_pool.log`

: Create a pool file based on pool.log file


### SEE ALSO ###

**libpmemblk(3)**, **libpmemlog(3)**, **pmempool(1)**

### PMEMPOOL ###

Part of the **pmempool(1)** suite.

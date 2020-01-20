---
layout: manual
Content-Style: 'text/css'
title: PMEMPOOL-DUMP
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

[comment]: <> (pmempool-dump.1 -- man page for pmempool-dump)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RANGE](#range)<br />
[EXAMPLE](#example)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmempool-dump** - dump user data from persistent memory pool

# SYNOPSIS #

```
$ pmempool dump [<options>] <file>
```

# DESCRIPTION #

The **pmempool** invoked with *dump* command dumps user data from specified pool file.
The output format may be either binary or hexadecimal.

By default the output format is hexadecimal.

By default data is dumped to standard output. It is possible to dump data to other
file by specifying **-o** option. In this case data will be appended to this file.

Using **-r** option you can specify number of blocks/bytes/data chunks using
special text format. See **RANGE** section for details.

##### Available options: #####

`-b, --binary`

Dump data in binary format.

`-r, --range <range>`

Range of pool file to dump. This may be number of blocks for **blk** pool
type or either number of bytes or number of data chunks for **log** pool type.

`-c, --chunk <size>`

Size of chunk for **log** pool type. See **pmemlog_walk**(3) in **libpmemlog**(7) for details.

`-o, --output <file>`

Name of output file.

`-h, --help`

Display help message and exit.

# RANGE #

Using **-r**, **--range** option it is possible to dump only a range of user data.
This section describes valid format of *\<range\>* string.

You can specify multiple ranges separated by commas.

`<first>-<last>`

All blocks/bytes/data chunks from *\<first\>* to *\<last\>* will be dumped.

`-<last>`

All blocks/bytes/data chunks up to *\<last\>* will be dumped.

`<first>-`

All blocks/bytes/data chunks starting from *\<first\>* will be dumped.

`<number>`

Only *\<number\>* block/byte/data chunk will be dumped.

# EXAMPLE #

```
$ pmempool dump pool.bin
```

Dump user data from pool.bin file to standard output

```
$ pmempool dump -o output.bin -r1,10-100 pool_blk.bin
```

Dump block number 1 and blocks from 10 to 100 from pool_blk.bin
containing pmem blk pool to output.bin file

```
$ pmempool dump -r 1K-2K pool.bin
```

Dump data form 1K to 2K from pool.bin file.

# SEE ALSO #

**pmempool**(1), **libpmemblk**(7), **libpmemlog**(7),
**libpmemobj**(7) and **<https://pmem.io>**

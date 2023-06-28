---
draft: false
slider_enable: true
description: ""
disclaimer: "The contents of this web site and the associated <a href=\"https://github.com/pmem\">GitHub repositories</a> are BSD-licensed open source."
aliases: ["pmempool-dump.1.html"]
title: "pmempool | PMDK"
header: "pmem Tools version 1.5"
---

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2016-2023, Intel Corporation)

[comment]: <> (pmempool-dump.1 -- man page for pmempool-dump)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmempool-dump** - dump user data from persistent memory pool

# SYNOPSIS #

```
$ pmempool dump [<options>] <file>
```

# NOTE #

> NOTICE:

The **obj** pool type is not supported by the dump command.

> NOTICE:

The dump command is left only for backward compatibility.

# DESCRIPTION #

The **pmempool** invoked with *dump* command dumps user data from specified pool file.
The output format may be either binary or hexadecimal.

By default the output format is hexadecimal.

By default data is dumped to standard output. It is possible to dump data to other
file by specifying **-o** option. In this case data will be appended to this file.

Using **-r** option you can specify number of data chunks using
special text format. See **RANGE** section for details.

##### Available options: #####

`-b, --binary`

Dump data in binary format.

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

# SEE ALSO #

**pmempool**(1), **libpmemobj**(7) and **<https://pmem.io>**

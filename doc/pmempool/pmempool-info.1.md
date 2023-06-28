---
draft: false
slider_enable: true
description: ""
disclaimer: "The contents of this web site and the associated <a href=\"https://github.com/pmem\">GitHub repositories</a> are BSD-licensed open source."
aliases: ["pmempool-info.1.html"]
title: "pmempool | PMDK"
header: "pmem Tools version 1.4"
---

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2016-2023, Intel Corporation)

[comment]: <> (pmempool-info.1 -- man page for pmempool-info)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RANGE](#range)<br />
[STATISTICS](#statistics)<br />
[EXAMPLE](#example)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmempool-info** - show information about persistent memory pool

# SYNOPSIS #

```
$ pmempool info [<options>] <file>
```

# DESCRIPTION #

The **pmempool** invoked with *info* command analyzes an existing pool
created by **PMDK** libraries provided by **file** parameter.
The **file** can be either existing pool file, a part file or a poolset file.

The main task of this command is to print all usable information from
pool headers and user data in human readable format. It automatically recognizes the pool
type by parsing and analyzing the pool header. The recognition
is done by checking the signature in the pool header. The main job of *info* command
is to present internal data structures as they are stored in file but *not* for checking
consistency. For this purpose there is the **pmempool-check**(1) command available.

The **pmempool** with *info* command analyzes pool file as long as it is possible
regarding *correctness* of internal meta-data (correct offsets, sizes etc.).
If it is *not* possible to analyze the rest of the file, **pmempool** exits
with error code and prints appropriate error message.

Currently there is lack of interprocess synchronization for pool files,
so the *info* command should be invoked off-line. Using **pmempool** on pool file which
may be modified by another process may lead to unexpected errors in pool file.

**pmempool info** opens pool file in *read-only* mode so the file
will remain untouched after processing.

The *info* command may collect and print basic statistics about data usage.
The statistics are specific to the type of pool. See **STATISTICS** section for details.

Although the pool consistency is *not* checked by the *info* command,
it prints information about checksum errors and/or offsets errors.

##### Available options: #####

By default the *info* command of **pmempool** prints information about the most important
internal data structures from pool. The particular set of headers and meta-data depend on
pool type. The pool type is recognized automatically and appropriate information is displayed
in human-readable format.

To force processing specified file(s) as desired pool type use **-f** option with appropriate
name of pool type. The valid names off pool type is **obj**.
This option may be useful when the pool header is corrupted and automatic recognition of
pool type fails.

`-f, --force obj`

Force parsing pool as specified pool type.

>NOTE:
By default only pool headers and internal meta-data are displayed.
To display user data use **-d** option. Using **-r** option you can
specify number of objects using special text format.
See **RANGE** section for details. The range refers to
*object numbers* in case of pmem obj pool type.
See **EXAMPLES** section for an example of usage of these options.

`-d, --data`

Dump user data in hexadecimal format.

`-r, --range <range>`

Range of objects/zone headers/chunk headers/lanes.
See **RANGE** section for details about range format.

`-n, --human`

Print sizes in human-readable format with appropriate units (e.g. 4k, 8M, 16G)

`-x, --headers-hex`

Print pool's internal data in mixed format which consists of hexadecimal dump of
header's data and parsed format displayed in human-readable format. This
allows one to see how data is stored in file.

`-s, --stats`

Print pool's statistics. See **STATISTICS** section for details.

`-k, --bad-blocks=<yes|no>`

Print bad blocks found in the pool.

`-h, --help`

Display help message and exit.

>NOTE:
By default the *info* command displays all data blocks when **-d** options is used.
However it is possible to skip blocks marked with *zero* and/or *error* flags.
It is also possible to skip blocks which are *not* marked with any flag. Skipping
blocks has impact on blocks ranges (e.g. display 10 blocks marked with error flag
in the range from 0 to 10000) and statistics.

`-z, --skip-zeros`

Skip blocks marked with *zero* flag.

`-e, --skip-error`

Skip blocks marked with *error* flag.

`-u, --skip-no-flag`

Skip blocks *not* marked with any flag.

By default the *info* command displays pool header and **pmemobj** pool descriptor.
In order to print information about other data structures one of the
following options may be used.

`-l, --lanes [<range>]`

Print information about lanes. If range is not specified all lanes are displayed.
The range can be specified using **-r** option right after the **-l** option.
See **RANGE** section for details about range format.

`-R, --recovery`

Print information about only those lanes which require recovery process.
This option requires **-l**, **--lanes** option.

`-O, --object-store`

Print information about all allocated objects.

`-t, --types <range>`

Print information about allocated objects only from specified range of type numbers.
If **-s**, **--stats** option is specified the objects statistics refer to objects from
specified range of type numbers. This option requires **-O**, **--object-store** or **-s**,
**--stats** options. See **RANGE** section for details about range format.

`-E, --no-empty`

Ignore empty lists of objects. This option requires **-O**, **--object-store** option.

`-o, --root`

Print information about a root object.

`-A, --alloc-header`

Print object's allocation header. This option requires **-O**, **--object-store** or **-l**,
**--lanes** or **-o**, **--root** options.

`-a, --oob-header`

Print object's out of band header. This option requires **-O**, **--object-store** or **-l**,
**--lanes** or **-o**, **--root** options.

`-H, --heap`

Print information about **pmemobj** heap. By default only a heap header is displayed.

`-Z, --zones`

If the **-H**, **--heap** option is used, print information about zones from specified range.
If the **-O**, **--object-store** option is used, print information about objects only
from specified range of zones. This option requires **-O**, **--object-store**, **-H**, **--heap**
or **-s**, **--stats** options.
The range can be specified using **-r** option right after the **-Z** option.
See **RANGE** section for details about range format.

`-C, --chunks [<range>]`

If the **-H, --heap** option is used, print information about chunks from specified range.
By default information about chunks of types *used* , *free* and *run* are displayed.
If the **-O, --object-store** option is used, print information about objects from
specified range of chunks within a zone. This option requires **-O, --object-store**,
**-H, --heap** or **-s, --stats** options.
The range can be specified using **-r** option right after the **-C** option.
See **RANGE** section for details about range format.

`-T, --chunk-type used,free,run,footer`

Print only specified type(s) of chunks.
The multiple types may be specified separated by comma.
This option requires **-H, --heap** and **-C, --chunks** options.

`-b, --bitmap`

Print bitmap of used blocks in chunks of type run.
This option requires **-H, --heap** and **-C, --chunks** options.

`-p, --replica <num>`

Print information from *\<num\>* replica. The 0 value means the master pool file.

# RANGE #

Using **-r, --range** option it is possible to dump only a range of user data.
This section describes valid format of *\<range\>* string.

You can specify multiple ranges separated by commas.

`<first>-<last>`

All data chunks from *\<first\>* to *\<last\>* will be dumped.

`-<last>`

All data chunks up to *\<last\>* will be dumped.

`<first>-`

All data chunks starting from *\<first\>* will be dumped.

`<number>`

Only *\<number\>* data chunk will be dumped.

# STATISTICS #

Below is the description of statistical measures for *obk* pool type.

+ **Object store**

  + **Number of objects** - Total number of objects and number of objects per type number.
  + **Number of bytes** - Total number of bytes and number of bytes per type number.

+ **Heap**

  + **Number of zones** - Total number of zones in the pool.
  + **Number of used zones** - Number of used zones in the pool.

+ **Zone** The zone's statistics are presented for each zone separately and the aggregated results from all zones.

  + **Number of chunks** - Total number of chunks in the zone and number of chunks of specified type.
  + **Chunks size** - Total size of all chunks in the zone and sum of sizes of chunks of specified type.

+ **Allocation classes**

  + **Units** - Total number of units of specified class.
  + **Used units** - Number of used units of specified class.
  + **Bytes** - Total number of bytes of specified class.
  + **Used bytes** - Number of used bytes of specified class.
  + **Total bytes** - Total number of bytes of all classes.
  + **Total used bytes** - Total number of used bytes of all classes.

# EXAMPLE #

```
$ pmempool info pool.obj
```

Parse and print information about pool.obj pool file.

```
$ pmempool info -d pool.obj
```

Print information and data in hexadecimal dump format for the pool.obj file.

```
$ pmempool info -d -r10-100 -eu pool.obj
```

Print information from the pool.obj file. Dump data chunks from 10 to 100,
skip blocks marked with error flag and not marked with any flag.

# SEE ALSO #

**pmempool**(1), **libpmemobj**(7) and **<https://pmem.io>**

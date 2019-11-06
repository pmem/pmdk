---
layout: manual
Content-Style: 'text/css'
title: DAXIO
collection: daxio
header: PMDK
date: daxio version 1.4
...

[comment]: <> (Copyright 2018, Intel Corporation)

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

[comment]: <> (daxio.1 -- man page for daxio)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[OPTIONS](#options)<br />
[EXAMPLE](#example)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**daxio** - Perform I/O on Device DAX devices or zero a Device DAX device

# SYNOPSIS #

```
$ daxio [<options>]
```

# DESCRIPTION #

The daxio utility performs I/O on Device DAX devices or zero
a Device DAX device.  Since the standard I/O APIs (read/write) cannot be used
with Device DAX, data transfer is performed on a memory-mapped device.
The **daxio** may be used to dump Device DAX data to a file, restore data from
a backup copy, move/copy data to another device or to erase data from
a device.

There must be at least one Device DAX device involved either as the input
or output.  If input or output is not specified, it will default to stdin
or stdout respectively.

No length specified will default to input file/device length or to the
output file/device length, if input is a special char file or stdin.

For a Device DAX device, **daxio** will attempt to clear bad blocks within
the range of writes before performing the I/O (it can be turned off using
the '--clear-bad-blocks=no' option).

# OPTIONS #

`-i, --input`
Input device or file to read from.

`-o, --output`
Output device or file to write to.

`-z, --zero`
Zero the output device for *len* size, or the entire device if no
length was provided.  The output device must be a Device DAX device.

`-b, --clear-bad-blocks=<yes|no>`
Clear bad blocks within the range of writes before performing the I/O
(default: yes).

`-l, --len`
The length in bytes to perform the I/O.  To make passing in size easier
for kibi, mebi, gibi, and tebi bytes, *len* may include unit suffix.
The *len* format must be compliant with the format specified in IEC 80000-13,
IEEE 1541 or the Metric Interchange Format.  These standards accept SI units
with obligatory B - kB, MB, GB, ... (multiplier by 1000) suffixes,
and IEC units with optional "iB" - KiB, MiB, GiB, ..., K, M, G, ...
(multiplier by 1024) suffixes.

`-s, --seek`
The number of bytes to skip over on the output before performing a write.
The same suffixes are accepted as for *len*.

`-k, --skip`
The number of bytes to skip over on the input before performing a read.
The same suffixes are accepted as for *len*.

`-V, --version`

Prints the version of **daxio**.

`-h, --help`

Prints synopsis and list of options.

# EXAMPLE #

```
# daxio --zero /dev/dax1.0

# daxio --input=/dev/dax1.0 --output=/home/myfile --len=2M --seek=4096

# cat /dev/zero | daxio --output=/dev/dax1.0

# daxio --input=/dev/zero --output=/dev/dax1.0 --skip=4096
```

# SEE ALSO #

**daxctl**(1), **ndctl**(1)
and **<http://pmem.io>**

---
layout: manual
Content-Style: 'text/css'
title: _MP(PMEMBLK_SET_ZERO, 3)
collection: libpmemblk
header: PMDK
date: pmemblk API version 1.1
...

[comment]: <> (Copyright 2017-2018, Intel Corporation)

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

[comment]: <> (pmemblk_set_zero.3 -- man page for block management functions)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmemblk_set_zero**(), **pmemblk_set_error**() - block management functions

# SYNOPSIS #

```c
#include <libpmemblk.h>

int pmemblk_set_zero(PMEMblkpool *pbp, long long blockno);
int pmemblk_set_error(PMEMblkpool *pbp, long long blockno);
```

# DESCRIPTION #

The **pmemblk_set_zero**() function writes zeros to block number *blockno* in
persistent memory resident array of blocks *pbp*. Using this function is faster
than actually writing a block of zeros since **libpmemblk**(7) uses metadata to
indicate the block should read back as zero.

The **pmemblk_set_error**() function sets the error state for block number
*blockno* in persistent memory resident array of blocks *pbp*.
A block in the error state returns *errno* **EIO** when read.
Writing the block clears the error state and returns the block to normal use.

# RETURN VALUE #

On success,  **pmemblk_set_zero**() and **pmemblk_set_error**() return 0.
On error, they return -1 and set *errno* appropriately.

# SEE ALSO #

**libpmemblk**(7) and **<http://pmem.io>**

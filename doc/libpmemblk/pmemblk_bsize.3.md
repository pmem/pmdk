---
layout: manual
Content-Style: 'text/css'
title: _MP(PMEMBLK_BSIZE, 3)
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

[comment]: <> (pmemblk_bsize.3 -- man page for functions that check number of available blocks or usable space in block memory pool)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmemblk_bsize**(), **pmemblk_nblock**() - check number of available blocks or
usable space in block memory pool

# SYNOPSIS #

```c
#include <libpmemblk.h>

size_t pmemblk_bsize(PMEMblkpool *pbp);
size_t pmemblk_nblock(PMEMblkpool *pbp);
```

# DESCRIPTION #

The **pmemblk_bsize**() function returns the block size of the specified
block memory pool, that is, the value which was passed as *bsize* to
_UW(pmemblk_create). *pbp* must be a block memory pool handle as returned by
**pmemblk_open**(3) or **pmemblk_create**(3).

The **pmemblk_nblock**() function returns the usable space in the block memory
pool. *pbp* must be a block memory pool handle as returned by
**pmemblk_open**(3) or **pmemblk_create**(3).

# RETURN VALUE #

The **pmemblk_bsize**() function returns the block size of the specified block
memory pool.

The **pmemblk_nblock**() function returns the usable space in the block memory
pool, expressed as the number of blocks available.

# SEE ALSO #

**pmemblk_create**(3), **pmemblk_open**(3),
**libpmemblk**(7) and **<https://pmem.io>**

---
layout: manual
Content-Style: 'text/css'
title: _MP(PMEMPOOL_RM, 3)
collection: libpmempool
header: PMDK
date: pmempool API version 1.3
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

[comment]: <> (pmempool_rm.3 -- man page for pool set management functions)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />

# NAME #

_UW(pmempool_rm) - remove persistent memory pool

# SYNOPSIS #

```c
#include <libpmempool.h>

_UWFUNCR1(int, pmempool_rm, *path, int flags)
```

_UNICODE()

# DESCRIPTION #

The _UW(pmempool_rm) function removes the pool pointed to by *path*. The *path*
can point to a regular file, device dax or pool set file. If *path* is a pool
set file, _UW(pmempool_rm) will remove all part files from local replicas
using **unlink**(2)_WINUX(,=q=, and all remote replicas using **rpmem_remove**(3)
(see **librpmem**(7)),=e=) before removing the pool set file itself.

The *flags* argument determines the behavior of _UW(pmempool_rm).
It is either 0 or the bitwise OR of one or more of the following flags:

+ **PMEMPOOL_RM_FORCE** - Ignore all errors when removing part files from
local _WINUX(,or remote )replicas.

+ **PMEMPOOL_RM_POOLSET_LOCAL** - Also remove local pool set file.

_WINUX(,
+ **PMEMPOOL_RM_POOLSET_REMOTE** - Also remove remote pool set file.)

# RETURN VALUE #

On success, _UW(pmempool_rm) returns 0. On error, it returns -1 and sets
*errno* accordingly.

# SEE ALSO #

**rpmem_remove**(3), **unlink**(3), **libpmemlog**(7),
**libpmemobj**(7), **librpmem**(7) and **<https://pmem.io>**

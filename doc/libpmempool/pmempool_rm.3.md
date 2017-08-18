---
layout: manual
Content-Style: 'text/css'
title: PMEMPOOL_RM!3
header: NVM Library
date: pmempool API version 1.1
...

[comment]: <> (Copyright 2017, Intel Corporation)

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

!pmempool_rm -- remove persistent memory pool


# SYNOPSIS #

```c
#include <libpmempool.h>

!ifdef{WIN32}
{
int pmempool_rmU(const char *path, int flags);
int pmempool_rmW(const wchar_t *path, int flags);
}{
int pmempool_rm(const char *path, int flags);
}
```

!ifdef{WIN32}
{
>NOTE: NVML API supports UNICODE. If **NVML_UTF8_API** macro is defined then
basic API functions are expanded to UTF-8 API with postfix *U*,
otherwise they are expanded to UNICODE API with postfix *W*.
}


# DESCRIPTION #

The !pmempool_rm function removes pool pointed by *path*. The *path* can
point to either a regular file, device dax or pool set file. In case of pool
set file the !pmempool_rm will remove all part files from local replicas
using **unlink**(3) and all remote replicas (supported on Linux)
using **rpmem_remove**(3) function (see **librpmem**(7)),
before removing the pool set file itself.

The *flags* argument determines the behavior of !pmempool_rm function.
It is either 0 or the bitwise OR of one or more of the following flags:

+ **PMEMPOOL_RM_FORCE**
Ignore all errors when removing part files from local replicas or remote
replica.

+ **PMEMPOOL_RM_POOLSET_LOCAL**
Remove also local pool set file.

+ **PMEMPOOL_RM_POOLSET_REMOTE**
Remove also remote pool set file.

# RETURN VALUE #

The !pmempool_rm function returns either 0 on success or -1 in case of error
with proper *errno* set accordingly.


# SEE ALSO #

**rpmem_remove**(3), **unlink**(3), **libpmemlog**(7),
**libpmemobj**(7), **librpmem**(7) and **<http://pmem.io>**

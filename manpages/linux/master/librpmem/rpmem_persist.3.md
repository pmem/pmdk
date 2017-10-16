---
layout: manual
Content-Style: 'text/css'
title: RPMEM_PERSIST
collection: librpmem
header: NVM Library
date: rpmem API version 1.1
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

[comment]: <> (rpmem_persist.3 -- page for most commonly used librpmem functions)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />


# NAME #

**rpmem_persist**(), **rpmem_read**(),
-- functions to copy and read remote pools


# SYNOPSIS #

```c
#include <librpmem.h>

int rpmem_persist(RPMEMpool *rpp, size_t offset, size_t length, unsigned lane);
int rpmem_read(RPMEMpool *rpp, void *buff, size_t offset, size_t length, unsigned lane);
```


# DESCRIPTION #

The **rpmem_persist**() function copies data of given *length* at given
*offset* from the associated local memory pool and makes sure the data is
persistent on the remote node before the function returns. The remote node
is identified by the *rpp* handle which must be returned from either
**rpmem_open**(3) or **rpmem_create**(3) functions. The *offset* is relative
to the *pool_addr* specified in the **rpmem_open**(3) or **rpmem_create**(3)
function calls. The *offset* and *length* combined must not exceed the
*pool_size* passed to the **rpmem_open**(3) or **rpmem_create**(3) functions.
The **rpmem_persist**() operation is performed using given *lane* number.
The lane must be less than the value returned by **rpmem_open**(3) or
**rpmem_create**(3) through the *nlanes* argument (so it can take a value
from 0 to *nlanes* - 1).

The **rpmem_read**() function reads *length* bytes of data from remote pool
at *offset* and copies it to the buffer *buff*. The operation is performed on
a given lane. The lane must be less than the value returned by **rpmem_open**(3)
or **rpmem_create**(3) through the *nlanes* argument (so it can take a value
from 0 to *nlanes* - 1). The *rpp* must point to a remote pool opened or created
previously by **rpmem_open**(3) or **rpmem_create**(3) functions respectively.


# RETURN VALUE #

The **rpmem_persist**() function returns zero if the entire memory area was
made persistent on remote node, otherwise it returns non-zero value and sets
*errno* appropriately.

The **rpmem_read**() function returns 0 if the data was read entirely,
otherwise non-zero value is returned and *errno* set appropriately.


# SEE ALSO #

**rpmem_create**(3), **rpmem_open**(3), **rpmem_persist**(3),
**sysconf**(3), **limits.conf**(5), **libpmemobj**(7)
and **<http://pmem.io>**

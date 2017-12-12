---
layout: manual
Content-Style: 'text/css'
title: _MP(PMEMCTO_STRDUP, 3)
collection: libpmemcto
header: NVM Library
date: libpmemcto API version 1.0
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

[comment]: <> (pmemcto_strdup.3 -- man page for libpmemcto)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[ERRORS](#errors)<br />
[SEE ALSO](#see-also)<br />


# NAME #

pmemcto_strdup -- duplicate a string


# SYNOPSIS #

```c
#include <libpmemcto.h>

char *pmemcto_strdup(PMEMctopool *pcp, const char *s);
```


# DESCRIPTION #

The **pmemcto_strdup**() function provides the same semantics as **strdup**(3),
but operates on the memory pool *pcp* instead of the process heap supplied
by the system.  It returns a pointer to a new string which is a duplicate
of the string *s*.  Memory for the new string is obtained with
**pmemcto_malloc**(3), on the given memory pool, and can be freed with
**pmemcto_free**(3) on the same memory pool.


# RETURN VALUE #

On success, the **pmemcto_strdup**() function returns a pointer to
the duplicated string.  If **pmemcto_strdup**() is unable to satisfy the
allocation request, a NULL pointer is returned and *errno* is set appropriately.


# ERRORS #

**ENOMEM** Insufficient memory available to allocate duplicated string.


# SEE ALSO #

**pmemcto_malloc**(3), **pmemcto_wcsdup**(3),
**strdup**(3), **wcsdup**(3), **malloc**(3),
**jemalloc**(3), **libpmemcto**(7) and **<http://pmem.io>**

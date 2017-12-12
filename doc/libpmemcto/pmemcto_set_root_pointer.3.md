---
layout: manual
Content-Style: 'text/css'
title: _MP(PMEMCTO_SET_ROOT_POINTER, 3)
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

[comment]: <> (pmemcto_set_root_pointer.3 -- man page for libpmemcto)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />


# NAME #

pmemcto_set_root_pointer, pmemcto_get_root_pointer
-- set or obtain the root object pointer


# SYNOPSIS #

```c
#include <libpmemcto.h>

void pmemcto_set_root_pointer(PMEMctopool *pcp, void *ptr);
void *pmemcto_get_root_pointer(PMEMctopool *pcp);

```


# DESCRIPTION #

The root object of persistent memory pool is an entry point for all other
persistent objects allocated using the **libpmemcto**(7) APIs.  In other words,
every single object stored in persistent memory pool should have the root
object at the end of its reference path.
There is exactly one root object in each pool.

The **pmemcto_set_root_pointer**() function saves the pointer to the root
object in given pool.  The *ptr* must have been returned by a previous call
to **pmemcto_malloc**(3), **pmemcto_calloc**(3) or **pmemcto_realloc**(3)
for *the same pool of memory*.

The **pmemcto_get_root_pointer**() function returns the pointer to the root
object in given pool, or NULL if the root pointer was never set.


# RETURN VALUE #

The **pmemcto_set_root_pointer**() function returns no value.

The **pmemcto_get_root_pointer**() function returns the pointer to the root
object in given pool, or NULL if the root pointer was never set.


# SEE ALSO #

**libpmemcto**(7) and **<http://pmem.io>**

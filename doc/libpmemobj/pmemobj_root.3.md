---
layout: manual
Content-Style: 'text/css'
title: PMEMOBJ_ROOT!3
header: NVM Library
date: pmemobj API version 2.2
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

[comment]: <> (pmemobj_root.3 -- man page for root object management)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />


# NAME #

**pmemobj_root**(), **pmemobj_root_construct**()
**POBJ_ROOT**(), **pmemobj_root_size**() -- root object management


# SYNOPSIS #

```c
#include <libpmemobj.h>

PMEMoid pmemobj_root(PMEMobjpool *pop, size_t size);
PMEMoid pmemobj_root_construct(PMEMobjpool *pop, size_t size,
	pmemobj_constr constructor, void *arg);
POBJ_ROOT(PMEMobjpool *pop, TYPE)
size_t pmemobj_root_size(PMEMobjpool *pop);
```


# DESCRIPTION #

The root object of persistent memory pool is an entry point for all other
persistent objects allocated using the **libpmemobj** API. In other words,
every single object stored in persistent memory pool should have the root
object at the end of its reference path. It may be assumed that for each
persistent memory pool the root object always exists, and there is exactly
one root object in each pool.

The **pmemobj_root**() function takes object associated with the persistent
memory pool pointed by *pop*. If this is the first call to **pmemobj_root**()
and the root object does not exists yet, it is implicitly allocated in a
thread-safe manner, so if the function is called by more than one
thread simultaneously (with identical *size* value).
The size of the root object is guaranteed to be not less than the requested
*size*. If the requested size is larger than the current size, the root
object is automatically resized. In such case, the old data is preserved and
the extra space is zeroed.

The **pmemobj_root_construct**() performs the same actions as the **pmemobj_root**()
function, but instead of zeroing the newly allocated object a *constructor* function
is called. The constructor is also called on reallocations.

The **POBJ_ROOT**() macro works the same way as the **pmemobj_root**() function except
it returns value.

The **pmemobj_root_size**() function returns the current size of the root object
associated with the persistent memory pool pointed by *pop*.


# RETURN VALUE #

The **pmemobj_root**() function returns a handle to the root object
associated with the persistent memory pool pointed by *pop*.
The same root object handle is returned in all the threads.
The **pmemobj_root**() function shall not fail,
except for the case if the requested object size is larger than the maximum
allocation size supported for given pool, or if there is not enough free
space in the pool to satisfy the reallocation of the root object.
In such case, **OID_NULL** is returned.

If the **pmemobj_root_construct**() constructor returns non-zero value
the allocation is canceled, the *OID_NULL* value is returned from the
caller and *errno* is set to **ECANCELED**. The **pmemobj_root_size**()
can be used in the constructor to check whether it's the first call to the
constructor.

The **POBJ_ROOT**() returns a typed *OID* of type *TYPE* instead of
*PMEMoid* returned by **pmemobj_root**().

The **pmemobj_root_size**() function returns the current size of the root object
associated with the persistent memory pool pointed by *pop*. The returned size
is the largest value requested by any of the earlier **pmemobj_root**() calls.
0 is returned if the root object has not been allocated yet.


# SEE ALSO #

**libpmemobj**(7) and **<http://pmem.io>**

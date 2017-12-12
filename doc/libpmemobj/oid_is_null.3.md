---
layout: manual
Content-Style: 'text/css'
title: _MP(OID_IS_NULL, 3)
collection: libpmemobj
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

[comment]: <> (oid_is_null.3 -- man page for persistent object identifier and functions)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
_WINUX(,[NOTES](#notes)<br />)
[SEE ALSO](#see-also)<br />


# NAME #

**OID_IS_NULL**(), **OID_EQUALS**(),
**pmemobj_direct**(), **pmemobj_oid**(),
**pmemobj_type_num**(), **pmemobj_pool_by_oid**(),
**pmemobj_pool_by_ptr**() -- functions that allow mapping
operations between object addresses, object handles, oids or type numbers


# SYNOPSIS #

```c
#include <libpmemobj.h>

OID_IS_NULL(PMEMoid oid)
OID_EQUALS(PMEMoid lhs, PMEMoid rhs)

void *pmemobj_direct(PMEMoid oid);
PMEMoid pmemobj_oid(const void *addr); (EXPERIMENTAL)
uint64_t pmemobj_type_num(PMEMoid oid);
PMEMobjpool *pmemobj_pool_by_oid(PMEMoid oid);
PMEMobjpool *pmemobj_pool_by_ptr(const void *addr);
```


# DESCRIPTION #

Each object stored in a persistent memory pool is represented by an object
handle of type *PMEMoid*. In practice, such a handle is a unique Object
IDentifier (*OID*) of global scope, which means that two objects from
different pools will never have the same *OID*. The special **OID_NULL**
macro defines a NULL-like handle that does not represent any object.
The size of a single object is limited by **PMEMOBJ_MAX_ALLOC_SIZE**.
Thus an allocation with a requested size greater than this value will fail.

An *OID* cannot be used as a direct pointer to an object. Each time
the program attempts to read or write object data, it must obtain the current
memory address of the object by converting its *OID* into a pointer.

In contrast to the memory address, the *OID* value for given object does not
change during the life of an object (except for *realloc*), and remains
valid after closing and reopening the pool. For this reason, if an object
contains a reference to another persistent object, for example, to build
some kind of a linked data structure, the reference must be an *OID* and not
a memory address.

**pmemobj_direct**() returns a pointer to the *PMEMoid* object with
handle *oid*.

**pmemobj_oid**() returns a *PMEMoid* handle to the object pointed
to by *addr*.

**pmemobj_type_num**() returns the type number of the *PMEMoid* object with
handle *oid*.

**pmemobj_pool_by_oid**() returns a *PMEMobjpool*\* handle to the pool
containing the *PMEMoid* object with handle *oid*.

**pmemobj_pool_by_ptr**() returns a *PMEMobjpool*\* handle to the pool
containing the address *addr*.

At the time of allocation (or reallocation), each object may be assigned
a number representing its type. Such a *type number* may be used to arrange the
persistent objects based on their actual user-defined structure type, thus
facilitating implementation of a simple run-time type safety mechanism. This
also allows iterating through all the objects of a given type that are stored
in the persistent memory pool. See **pmemobj_first**(3) for more information.

The **OID_IS_NULL**() macro checks if *PMEMoid* represents a NULL object.

The **OID_EQUALS**() macro compares two *PMEMoid* objects.


# RETURN VALUE #

The **pmemobj_direct**() function returns a pointer to the object represented
by *oid*. If *oid* is **OID_NULL**, **pmemobj_direct**() returns NULL.

The **pmemobj_oid**() function returns a *PMEMoid* handle to the object pointed
to by *addr*. If *addr* is not from within a pmemobj pool, **OID_NULL** is
returned. If *addr* is not the start of an object (does not point to the
beginning of a valid allocation), the resulting *PMEMoid* can be safely used
only with:

+ **pmemobj_pool_by_oid**()

+ **pmemobj_direct**()

+ **pmemobj_tx_add_range**(3)

The **pmemobj_type_num**() function returns the type number of the object
represented by *oid*.

The **pmemobj_pool_by_oid**() function returns a handle to the pool that
contains the object represented by *oid*. If the the pool is not open or
*oid* is **OID_NULL**, **pmemobj_pool_by_oid**() returns NULL.

The **pmemobj_pool_by_ptr**() function returns a handle to the pool that
contains the address, or NULL if the address does not belong to any open pool.

_WINUX(,=q=

# NOTES #

For performance reasons, on Linux and FreeBSD the **pmemobj_direct**()
function is inlined by default. To use the non-inlined variant of
**pmemobj_direct**(), define **PMEMOBJ_DIRECT_NON_INLINE** prior
to the *\#include* of **\<libpmemobj.h\>**, either with *\#define* or with
the *\-D* option to the compiler.=e=)


# SEE ALSO #

**libpmemobj**(7) and **<http://pmem.io>**

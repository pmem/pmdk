---
layout: manual
Content-Style: 'text/css'
title: _MP(TOID_DECLARE, 3)
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

[comment]: <> (toid_declare.3 -- man page for obj type safety mechanism)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[SEE ALSO](#see-also)<br />


# NAME #

**TOID_DECLARE**(), **TOID_DECLARE_ROOT**(), **TOID**(),
**TOID_TYPE_NUM**(), **TOID_TYPE_NUM_OF**(), **TOID_VALID**(),
**OID_INSTANCEOF**(), **TOID_ASSIGN**(), **TOID_IS_NULL**(),
**TOID_EQUALS**(), **TOID_TYPEOF**(), **TOID_OFFSETOF**(),
**DIRECT_RW**(), **D_RW**(), **DIRECT_RO**(),
**D_RO**() -- libpmemobj type safety mechanism


# SYNOPSIS #

```c
#include <libpmemobj.h>

TOID_DECLARE(TYPE, uint64_t type_num)
TOID_DECLARE_ROOT(ROOT_TYPE)
TOID(TYPE)
TOID_TYPE_NUM(TYPE)
TOID_TYPE_NUM_OF(TOID oid)
TOID_VALID(TOID oid)
OID_INSTANCEOF(PMEMoid oid, TYPE)
TOID_ASSIGN(TOID o, VALUE)
TOID_IS_NULL(TOID o)
TOID_EQUALS(TOID lhs, TOID rhs)
TOID_TYPEOF(TOID o)
TOID_OFFSETOF(TOID o, FILED)
DIRECT_RW(TOID oid)
D_RW(TOID oid)
DIRECT_RO(TOID oid)
D_RO(TOID oid)
```


# DESCRIPTION #

Operating on untyped object handles, as well as on direct untyped object
pointers (*void\**), may be confusing and error-prone. To facilitate
type safety, **libpmemobj**(7) defines a set of macros that provide static
type enforcement, catching potential errors at compile time. For example,
a compile-time error is generated when an attempt is made to assign a handle to
an object of one type to the object handle variable of another type of object.

The **TOID_DECLARE**() macro declares a typed *OID* of user-defined type
*TYPE* and type number *type_num*.

The **TOID_DECLARE_ROOT**() macro declares a typed *OID* of user-defined type
*ROOT_TYPE* and root object type number **POBJ_ROOT_TYPE_NUM**.

The **TOID**() macro declares a handle to an object of type *TYPE*,
where *TYPE* is the name of a user-defined structure. The typed *OID* must
be declared first using the **TOID_DECLARE**(), **TOID_DECLARE_ROOT**(),
**POBJ_LAYOUT_TOID**(3) or **POBJ_LAYOUT_ROOT**(3) macros.

The **TOID_TYPE_NUM**() macro returns the type number of the type specified
by *TYPE*.

The **TOID_TYPE_NUM_OF**() macro returns the type number of the object
specified by *oid*. The type number is read from the typed *OID*.

The **TOID_VALID**() macro validates whether the type number stored in
the object's metadata is equal to the type number read from the typed *OID*.

The **OID_INSTANCEOF**() macro checks whether the *oid* is of type *TYPE*.

The **TOID_ASSIGN**() macro assigns the object handle *VALUE* to typed *OID*
*o*.

The **TOID_IS_NULL**() macro evaluates to true if the object handle
represented by *o* is **OID_NULL**.

The **TOID_EQUALS**() macro evaluates to true if both the *lhs* and *rhs*
object handles reference the same persistent object.

The **TOID_TYPEOF**() macro returns the type of the object handle represented
by typed *OID* *o*.

The **TOID_OFFSETOF**() macro returns the offset of the *FIELD* member from
the start of the object represented by *o*.

The **DIRECT_RW**() macro and its shortened form **D_RW**() return a typed
write pointer (*TYPE\**) to an object represented by *oid*. If *oid* is
**OID_NULL**, the macro evaluates to NULL.

The **DIRECT_RO**() macro and its shortened form **D_RO**() return a typed
read-only (const) pointer (*TYPE\**) to an object represented by *oid*. If
*oid* is **OID_NULL**, the macro evaluates to NULL.


# SEE ALSO #

**OID_IS_NULL**(3), **POBJ_LAYOUT_ROOT**(3), **POBJ_LAYOUT_TOID**(3),
**libpmemobj**(7) and **<http://pmem.io>**

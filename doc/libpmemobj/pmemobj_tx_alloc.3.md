---
layout: manual
Content-Style: 'text/css'
title: _MP(PMEMOBJ_TX_ALLOC, 3)
collection: libpmemobj
header: PMDK
date: pmemobj API version 2.3
...

[comment]: <> (Copyright 2017-2019, Intel Corporation)

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

[comment]: <> (pmemobj_tx_alloc.3 -- man page for transactional object manipulation)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmemobj_tx_alloc**(), **pmemobj_tx_zalloc**(),
**pmemobj_tx_xalloc**(), **pmemobj_tx_realloc**(),
**pmemobj_tx_zrealloc**(), **pmemobj_tx_strdup**(),
**pmemobj_tx_xstrdup**(), **pmemobj_tx_wcsdup**(),
**pmemobj_tx_xwcsdup**(), **pmemobj_tx_free**(),
**pmemobj_tx_xfree**()

**TX_NEW**(), **TX_ALLOC**(),
**TX_ZNEW**(), **TX_ZALLOC**(),
**TX_XALLOC**(), **TX_REALLOC**(),
**TX_ZREALLOC**(), **TX_STRDUP**(),
**TX_XSTRDUP**(), **TX_WCSDUP**(),
**TX_XWCSDUP**(), **TX_FREE**(),
**TX_XFREE**()
- transactional object manipulation

# SYNOPSIS #

```c
#include <libpmemobj.h>

PMEMoid pmemobj_tx_alloc(size_t size, uint64_t type_num);
PMEMoid pmemobj_tx_zalloc(size_t size, uint64_t type_num);
PMEMoid pmemobj_tx_xalloc(size_t size, uint64_t type_num, uint64_t flags);
PMEMoid pmemobj_tx_realloc(PMEMoid oid, size_t size, uint64_t type_num);
PMEMoid pmemobj_tx_zrealloc(PMEMoid oid, size_t size, uint64_t type_num);
PMEMoid pmemobj_tx_strdup(const char *s, uint64_t type_num);
PMEMoid pmemobj_tx_wcsdup(const wchar_t *s, uint64_t type_num);
int pmemobj_tx_free(PMEMoid oid);
int pmemobj_tx_xfree(PMEMoid oid, uint64_t flags);

TX_NEW(TYPE)
TX_ALLOC(TYPE, size_t size)
TX_ZNEW(TYPE)
TX_ZALLOC(TYPE, size_t size)
TX_XALLOC(TYPE, size_t size, uint64_t flags)
TX_REALLOC(TOID o, size_t size)
TX_ZREALLOC(TOID o, size_t size)
TX_STRDUP(const char *s, uint64_t type_num)
TX_WCSDUP(const wchar_t *s, uint64_t type_num)
TX_FREE(TOID o)
TX_XFREE(TOID o, uint64_t flags)
```

# DESCRIPTION #

The **pmemobj_tx_alloc**() function transactionally allocates a new object of
given *size* and *type_num*. In contrast to the non-transactional allocations,
the objects are added to the internal object containers of given *type_num*
only after the transaction is committed, making the objects visible to the
**POBJ_FOREACH_\***() macros. This function must be called during
**TX_STAGE_WORK**.

The **pmemobj_tx_zalloc**() function transactionally allocates a new zeroed
object of given *size* and *type_num*. This function must be called during
**TX_STAGE_WORK**.

The **pmemobj_tx_xalloc**() function transactionally allocates a new object
of given *size* and *type_num*. The *flags* argument is a bitmask of the
following values:

+ **POBJ_XALLOC_ZERO** - zero the allocated object (equivalent of pmemobj_tx_zalloc)

+ **POBJ_XALLOC_NO_FLUSH** - skip flush on commit
(when application deals with flushing or uses pmemobj_memcpy_persist)

+ **POBJ_CLASS_ID(class_id)** - allocate an object from the allocation
class with id equal to *class_id*

+ **POBJ_ARENA_ID(arena_id)** - allocate an object from the arena specified by
*arena_id*. The arena must exist, otherwise, the behavior is undefined.
If *arena_id* is equal 0, then arena assigned to the current thread will be used.

+ **POBJ_XALLOC_NO_ABORT** - if the function does not end successfully,
do not abort the transaction.

This function must be called during **TX_STAGE_WORK**.

The **pmemobj_tx_realloc**() function transactionally resizes an existing
object to the given *size* and changes its type to *type_num*. If *oid* is
**OID_NULL**, then the call is equivalent to *pmemobj_tx_alloc(pop, size,
type_num)*. If *size* is equal to zero and *oid* is not **OID_NULL**, then
the call is equivalent to *pmemobj_tx_free(oid)*. If the new size is larger
than the old size, the added memory will *not* be initialized. This function
must be called during **TX_STAGE_WORK**.

The **pmemobj_tx_zrealloc**() function transactionally resizes an existing
object to the given *size* and changes its type to *type_num*. If the new
size is larger than the old size, the extended new space is zeroed. This
function must be called during **TX_STAGE_WORK**.

The **pmemobj_tx_strdup**() function transactionally allocates a new object
containing a duplicate of the string *s* and assigns it a type *type_num*.
This function must be called during **TX_STAGE_WORK**.

The **pmemobj_tx_xstrdup**() function behaves exactly the same as
**pmemobj_tx_strdup**() when *flags* equals zero.
The *flags* argument is a bitmask of values described in **pmemobj_tx_xalloc** section.

The **pmemobj_tx_wcsdup**() function transactionally allocates a new object
containing a duplicate of the wide character string *s* and assigns it a type
*type_num*. This function must be called during **TX_STAGE_WORK**.

The **pmemobj_tx_xwcsdup**() function behaves exactly the same as
**pmemobj_tx_wcsdup**() when *flags* equals zero.
The *flags* argument is a bitmask of values described in **pmemobj_tx_xalloc** section.

The **pmemobj_tx_free**() function transactionally frees an existing object
referenced by *oid*. This function must be called during **TX_STAGE_WORK**.

The **pmemobj_tx_xfree**() function behaves exactly the same as
**pmemobj_tx_free**() when *flags* equals zero.
*flags* is a bitmask of the following value:

+ **POBJ_XFREE_NO_ABORT** - if the function does not end successfully,
do not abort the transaction.

This function must be called during **TX_STAGE_WORK**.

The **TX_NEW**() macro transactionally allocates a new object of given *TYPE*
and assigns it a type number read from the typed *OID*. The allocation size is
determined from the size of the user-defined structure *TYPE*. If successful
and called during **TX_STAGE_WORK** it returns a handle to the newly allocated
object. Otherwise, the stage is changed to **TX_STAGE_ONABORT**, **OID_NULL**
is returned, and *errno* is set appropriately.

The **TX_ALLOC**() macro transactionally allocates a new object of given *TYPE*
and assigns it a type number read from the typed *OID*. The allocation size is
passed by *size* parameter. If successful and called during **TX_STAGE_WORK**
it returns a handle to the newly allocated object. Otherwise, the stage is set
to **TX_STAGE_ONABORT**, **OID_NULL** is returned, and *errno* is set
appropriately.

The **TX_ZNEW**() macro transactionally allocates a new zeroed object of given
*TYPE* and assigns it a type number read from the typed *OID*. The allocation
size is determined from the size of the user-defined structure *TYPE*. If
successful and called during **TX_STAGE_WORK** it returns a handle to the newly
allocated object. Otherwise, stage changes to **TX_STAGE_ONABORT**, **OID_NULL**
is returned, and *errno* is set appropriately.

The **TX_ZALLOC**() macro transactionally allocates a new zeroed object of
given *TYPE* and assigns it a type number read from the typed *OID*. The
allocation size is passed by *size* argument. If successful and called during
**TX_STAGE_WORK** it returns a handle to the newly allocated object. Otherwise,
the stage is changed to **TX_STAGE_ONABORT**, **OID_NULL** is returned, and
*errno* is set appropriately.

The **TX_XALLOC**() macro transactionally allocates a new object of given
*TYPE* and assigns it a type number read from the typed *OID*. The allocation
size is passed by *size* argument. The *flags* argument is a bitmask of values
described in **pmemobj_tx_xalloc** section. If successful and called during
**TX_STAGE_WORK** it returns a handle to the newly allocated object. Otherwise,
the **OID_NULL** is returned, **errno** is set and when flags do not
contain **POBJ_XALLOC_NO_ABORT**, the transaction is aborted.

The **TX_REALLOC**() macro transactionally resizes an existing object
referenced by a handle *o* to the given *size*. If successful and called during
**TX_STAGE_WORK** it returns a handle to the reallocated object. Otherwise, the
stage is changed to **TX_STAGE_ONABORT**, **OID_NULL** is returned, and *errno*
is set appropriately.

The **TX_ZREALLOC**() macro transactionally resizes an existing object
referenced by a handle *o* to the given *size*. If the new size is larger than
the old size, the extended new space is zeroed. If successful and called during
**TX_STAGE_WORK** it returns a handle to the reallocated object. Otherwise, the
stage is changed to **TX_STAGE_ONABORT**, **OID_NULL** is returned, and *errno*
is set appropriately.

The **TX_STRDUP**() macro transactionally allocates a new object containing a
duplicate of the string *s* and assigns it type *type_num*. If successful
and called during **TX_STAGE_WORK** it returns a handle to the newly allocated
object. Otherwise, the stage is changed to **TX_STAGE_ONABORT**, **OID_NULL**
is returned, and *errno* is set appropriately.

The **TX_XSTRDUP**() macro transactionally allocates a new object containing a
duplicate of the string *s* and assigns it type *type_num*. The *flags* argument
is a bitmask of values described in **pmemobj_tx_xalloc** section. If successful and
called during **TX_STAGE_WORK** it returns a handle to the newly allocated
object. Otherwise, the **OID_NULL** is returned, **errno** is set and when flags
do not contain **POBJ_XALLOC_NO_ABORT**, the transaction is aborted.

The **TX_WCSDUP**() macro transactionally allocates a new object containing a
duplicate of the wide character string *s* and assigns it a type *type_num*. If
successful and called during **TX_STAGE_WORK**, it returns a handle to the
newly allocated object. Otherwise, the stage is changed to **TX_STAGE_ONABORT**,
**OID_NULL** is returned, and *errno* is set appropriately.

The **TX_XWCSDUP**() macro transactionally allocates a new object containing a
duplicate of the wide character string *s* and assigns it a type *type_num*.
The *flags* argument is a bitmask of values described in **pmemobj_tx_xalloc** section.
If successful and called during **TX_STAGE_WORK** it returns a handle to the
newly allocated object. Otherwise, the **OID_NULL** is returned, **errno** is set
and when flags do not contain **POBJ_XALLOC_NO_ABORT**, the transaction is aborted.

The **TX_FREE**() macro transactionally frees the memory space represented by
an object handle *o*. If *o* is **OID_NULL**, no operation is performed. If
successful and called during **TX_STAGE_WORK**, **TX_FREE**() returns 0.
Otherwise, the stage is changed to **TX_STAGE_ONABORT** and *errno* is set appropriately.

The **TX_XFREE**() macro transactionally frees the memory space represented by
an object handle *o*. If *o* is **OID_NULL**, no operation is performed.
The *flags* argument is a bitmask of values described in **pmemobj_tx_xfree**
section. If successful and called during **TX_STAGE_WORK**, **TX_FREE**()
returns 0. Otherwise, the error number is returned, **errno** is set and when
flags do not contain **POBJ_XFREE_NO_ABORT**, the transaction is aborted.

# RETURN VALUE #

On success, the **pmemobj_tx_alloc**(), **pmemobj_tx_zalloc**(),
**pmemobj_tx_strdup**() and **pmemobj_tx_wcsdup**()
functions return a handle to the newly allocated object. Otherwise, the stage
is changed to **TX_STAGE_ONABORT**, **OID_NULL** is returned, and *errno* is
set appropriately. If *size* equals 0, **OID_NULL** is returned and *errno* is
set appropriately.

On success, the **pmemobj_tx_xalloc**(), **pmemobj_tx_xstrdup**() and
**pmemobj_tx_xwcsdup**() functions return a handle to the newly allocated object.
Otherwise, the **OID_NULL** is returned, **errno** is set and when flags do not
contain **POBJ_XALLOC_NO_ABORT**, the transaction is aborted.

On success, **pmemobj_tx_realloc**() and **pmemobj_tx_zrealloc**() return
a handle to the resized object. Otherwise, the stage is changed to
**TX_STAGE_ONABORT**, **OID_NULL** is returned, and *errno* is set
appropriately. Note that the object handle value may change as a result of
reallocation.

On success, **pmemobj_tx_free**() returns 0. Otherwise, the stage is changed
to **TX_STAGE_ONABORT**, **errno** is set appropriately and transaction is aborted

On success **pmemobj_tx_xfree**() returns 0. Otherwise, the error number is
returned, **errno** is set and when flags do not contain **POBJ_XFREE_NO_ABORT**,
the transaction is aborted.

# SEE ALSO #

**pmemobj_tx_add_range**(3), **pmemobj_tx_begin**(3),
**libpmemobj**(7) and **<http://pmem.io>**

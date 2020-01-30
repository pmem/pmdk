---
layout: manual
Content-Style: 'text/css'
title: _MP(PMEMOBJ_ALLOC, 3)
collection: libpmemobj
header: PMDK
date: pmemobj API version 2.3
...

[comment]: <> (Copyright 2017-2020, Intel Corporation)

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

[comment]: <> (pmemobj_alloc.3 -- man page for non-transactional atomic allocations)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmemobj_alloc**(), **pmemobj_xalloc**(), **pmemobj_zalloc**(),
**pmemobj_realloc**(), **pmemobj_zrealloc**(), **pmemobj_strdup**(),
**pmemobj_wcsdup**(), **pmemobj_alloc_usable_size**(), **pmemobj_defrag**(),
**POBJ_NEW**(), **POBJ_ALLOC**(), **POBJ_ZNEW**(), **POBJ_ZALLOC**(),
**POBJ_REALLOC**(), **POBJ_ZREALLOC**(), **POBJ_FREE**()
- non-transactional atomic allocations

# SYNOPSIS #

```c
#include <libpmemobj.h>

typedef int (*pmemobj_constr)(**PMEMobjpool *pop, void *ptr, void *arg);
int pmemobj_alloc(PMEMobjpool *pop, PMEMoid *oidp, size_t size,
	uint64_t type_num, pmemobj_constr constructor, void *arg);
int pmemobj_xalloc(PMEMobjpool *pop, PMEMoid *oidp, size_t size,
	uint64_t type_num, uint64_t flags, pmemobj_constr constructor,
	void *arg); (EXPERIMENTAL)
int pmemobj_zalloc(PMEMobjpool *pop, PMEMoid *oidp, size_t size,
	uint64_t type_num);
void pmemobj_free(PMEMoid *oidp);
int pmemobj_realloc(PMEMobjpool *pop, PMEMoid *oidp, size_t size,
	uint64_t type_num);
int pmemobj_zrealloc(PMEMobjpool *pop, PMEMoid *oidp, size_t size,
	uint64_t type_num);
int pmemobj_strdup(PMEMobjpool *pop, PMEMoid *oidp, const char *s,
	uint64_t type_num);
int pmemobj_wcsdup(PMEMobjpool *pop, PMEMoid *oidp, const wchar_t *s,
	uint64_t type_num);
size_t pmemobj_alloc_usable_size(PMEMoid oid);
int pmemobj_defrag(PMEMobjpool *pop, PMEMoid **oidv, size_t oidcnt,
	struct pobj_defrag_result *result);

POBJ_NEW(PMEMobjpool *pop, TOID *oidp, TYPE, pmemobj_constr constructor,
	void *arg)
POBJ_ALLOC(PMEMobjpool *pop, TOID *oidp, TYPE, size_t size,
	pmemobj_constr constructor, void *arg)
POBJ_ZNEW(PMEMobjpool *pop, TOID *oidp, TYPE)
POBJ_ZALLOC(PMEMobjpool *pop, TOID *oidp, TYPE, size_t size)
POBJ_REALLOC(PMEMobjpool *pop, TOID *oidp, TYPE, size_t size)
POBJ_ZREALLOC(PMEMobjpool *pop, TOID *oidp, TYPE, size_t size)
POBJ_FREE(TOID *oidp)
```

# DESCRIPTION #

Functions described in this document provide the mechanism to allocate,
resize and free objects from the persistent memory pool in a thread-safe
and fail-safe manner. All the routines are atomic with respect to other threads
and any power-fail interruptions. If any of these operations is torn by program
failure or system crash, on recovery they are guaranteed to be entirely completed
or discarded, leaving the persistent memory heap and internal object containers
in a consistent state.

All these functions should be used outside transactions. If executed within
an open transaction they are considered durable immediately after completion.
Changes made with these functions will not be rolled back if the transaction
is aborted or interrupted. They have no information about other changes made
by transactional API, so if the same data is modified in a single transaction
using transactional and then non-transactional API, transaction abort
will likely corrupt the data.

The allocations are always aligned to a cache-line boundary.

The *pmemobj_constr* type represents a constructor for atomic allocation
from the persistent memory heap associated with memory pool *pop*. *ptr*
is a pointer to the allocated memory area and *arg* is a user-defined
argument passed to the constructor.

The **pmemobj_alloc**() function allocates a new object from the persistent
memory heap associated with memory pool *pop*. The *PMEMoid* of the allocated
object is stored in *oidp*. If *oidp* is NULL, then the newly allocated object
may be accessed only by iterating objects in the object container associated
with the type number *type_num*, as described in **POBJ_FOREACH**(3). If *oidp*
points to a memory location from the **pmemobj** heap, *oidp* is modified
atomically. Before returning, **pmemobj_alloc**() calls the *constructor*
function, passing the pool handle *pop*, the pointer to the newly allocated
object in *ptr*, and the *arg* argument. It is guaranteed that the
allocated object is either properly initialized, or if the allocation
is interrupted before the constructor completes, the memory space reserved
for the object is reclaimed. *size* can be any non-zero value; however,
due to internal padding and object metadata, the actual size of the allocation
will differ from the requested size by at least 64 bytes. For this reason,
making allocations of a size less than 64 bytes is extremely inefficient
and discouraged. The allocated object is added to the internal container
associated with *type_num*.

**pmemobj_xalloc**() is equivalent to **pmemobj_alloc**(), but with an
additional *flags* argument that is a bitmask of the following values:

+ **POBJ_XALLOC_ZERO** - zero the allocated object (equivalent of **pmemobj_zalloc**())

+ **POBJ_CLASS_ID(class_id)** - allocate an object from the allocation class
*class_id*. The class id cannot be 0.

+ **POBJ_ARENA_ID(arena_id)** - allocate an object from the arena specified by
*arena_id*. The arena must exist, otherwise, the behavior is undefined.
If *arena_id* is equal 0, then arena assigned to the current thread will be used.

The **pmemobj_zalloc**() function allocates a new zeroed object from
the persistent memory heap associated with memory pool *pop*. The *PMEMoid*
of the allocated object is stored in *oidp*. If *oidp* is NULL, then
the newly allocated object may be accessed only by iterating objects in the
object container associated with the type number *type_num*, as described in
**POBJ_FOREACH**(3). If *oidp* points to a memory location from the **pmemobj**
heap, *oidp* is modified atomically. *size* can be any non-zero value;
however, due to internal padding and object metadata, the actual size
of the allocation will differ from the requested one by at least 64 bytes.
For this reason, making allocations of a size less than 64 bytes is extremely
inefficient and discouraged. The allocated object is added to the internal
container associated with *type_num*.

The **pmemobj_free**() function frees the memory space represented by *oidp*,
which must have been allocated by a previous call to **pmemobj_alloc**(),
**pmemobj_xalloc**(), **pmemobj_zalloc**(), **pmemobj_realloc**(), or
**pmemobj_zrealloc**(). **pmemobj_free**() provides the same semantics as
**free**(3), but instead of operating on the process heap supplied by the
system, it operates on the persistent memory heap. If *oidp* is **OID_NULL**,
no operation is performed. If *oidp* is NULL or if it points to the root
object's *OID*, the behavior of **pmemobj_free**() is undefined. *oidp* is
set to **OID_NULL** after the memory is freed. If *oidp* points to a memory
location from the **pmemobj** heap, *oidp* is modified atomically.

The **pmemobj_realloc**() function changes the size of the object represented
by *oidp* to *size* bytes. **pmemobj_realloc**() provides similar semantics to
**realloc**(3), but operates on the persistent memory heap associated with
memory pool *pop*. The resized object is also added or moved to the internal
container associated with type number *type_num*. The contents will be
unchanged in the range from the start of the region up to the minimum of the
old and new sizes. If the new size is larger than the old size, the added
memory will *not* be initialized. If *oidp* is *OID_NULL*, then the call is
equivalent to *pmemobj_alloc(pop, size, type_num)*. If *size* is equal to zero,
and *oidp* is not **OID_NULL**, then the call is equivalent to
*pmemobj_free(oid)*. Unless *oidp* is **OID_NULL**, it must have been allocated
by an earlier call to **pmemobj_alloc**(), **pmemobj_xalloc**(),
**pmemobj_zalloc**(), **pmemobj_realloc**(), or **pmemobj_zrealloc**(). Note
that the object handle value may change as a result of reallocation. If the
object was moved, the memory space represented by *oid* is reclaimed. If
*oidp* points to a memory location from the **pmemobj** heap, *oidp* is
modified atomically. If *oidp* is NULL or if it points to the root
object's *OID*, the behavior of **pmemobj_realloc**() is undefined.

**pmemobj_zrealloc**() is equivalent to **pmemobj_realloc**(), except that
if the new size is larger than the old size, the added memory will be zeroed.

The **pmemobj_strdup**() function stores a handle to a new object in *oidp*
which is a duplicate of the string *s*. **pmemobj_strdup**() provides the
same semantics as **strdup**(3), but operates on the persistent memory heap
associated with memory pool *pop*. If *oidp* is NULL, then the newly allocated
object may be accessed only by iterating objects in the object container
associated with type number *type_num*, as described in **POBJ_FOREACH**(3).
If *oidp* points to a memory location from the **pmemobj** heap, *oidp*
is modified atomically. The allocated string object is also added to the
internal container associated with type number *type_num*. Memory for the new
string is obtained with **pmemobj_alloc**(), on the given memory pool, and can
be freed with **pmemobj_free**() on the same memory pool.

**pmemobj_wcsdup**() is equivalent to **pmemobj_strdup**(), but operates on
a wide character string (wchar_t) rather than a standard character string.

The **pmemobj_alloc_usable_size**() function provides the same semantics as
**malloc_usable_size**(3), but instead of the process heap supplied by the
system, it operates on the persistent memory heap.

The **POBJ_NEW**() macro is a wrapper around the **pmemobj_alloc**() function.
Instead of taking a pointer to *PMEMoid*, it takes a pointer to the typed *OID*
of type name *TYPE*, and passes the size and type number from the typed *OID*
to **pmemobj_alloc**().

The **POBJ_ALLOC**() macro is equivalent to **POBJ_NEW**, except that instead
of using the size of the typed *OID*, passes *size* to **pmemobj_alloc**().

The **POBJ_ZNEW**() macro is a wrapper around the **pmemobj_zalloc**()
function. Instead of taking a pointer to *PMEMoid*, it takes a pointer to the
typed *OID* of type name *TYPE*, and passes the size and type number from
the typed *OID* to **pmemobj_zalloc**().

The **POBJ_ZALLOC**() macro is equivalent to **POBJ_ZNEW**, except that instead
of using the size of the typed *OID*, passes *size* to **pmemobj_zalloc**().

The **POBJ_REALLOC**() macro is a wrapper around the **pmemobj_realloc**()
function. Instead of taking a pointer to *PMEMoid*, it takes a pointer to the
typed *OID* of type name *TYPE*, and passes the type number from the typed
*OID* to **pmemobj_realloc**().

The **POBJ_ZREALLOC**() macro is a wrapper around the **pmemobj_zrealloc**()
function. Instead of taking a pointer to *PMEMoid*, it takes a pointer to the
typed *OID* of type name *TYPE*, and passes the type number from the typed
*OID* to **pmemobj_zrealloc**().

The **POBJ_FREE**() macro is a wrapper around the **pmemobj_free**() function
which takes a pointer to the typed *OID* instead of to *PMEMoid*.

The **pmemobj_defrag**() function performs defragmentation
on the objects provided through the array of pointers to PMEMoids *oidv*
with size *oidcnt*. If an object from the provided array is selected to be moved
to a new location in the heap, it is reallocated and all provided pointers
to that object are atomically updated.
To maintain data structure consistency, applications should always provide
all pointers for an object to **pmemobj_defrag** method. This ensures that,
even in the presence of failures, all pointers to the object will either point
to the old or a new location.
All objects and pointers to objects should belong to the pool *pop* or,
in case of pointers, can also reside in volatile memory.
Defragmentation across pools is not supported.
Objects in the array that are *OID_NULL* are skipped over and no operation
is performed on them. All other objects must have been allocated
by an earlier call to **pmemobj_alloc**(), **pmemobj_xalloc**(),
**pmemobj_zalloc**(), **pmemobj_realloc**(), **pmemobj_zrealloc**(),
**pmemobj_strdup**() or **pmemobj_wcsdup**().
The *result* variable is an instance of *struct pobj_defrag_result* and,
if not NULL, can be used to read *total*, the number of objects found that
were processed, and *relocated*, the number of objects that were
relocated during defragmentation. These variables are always initialized and
can be non-zero, even if the return value of **pmemobj_defrag**() indicated a
failure. This is because the failure might have occurred after some objects were
already processed.

# RETURN VALUE #

On success, **pmemobj_alloc**() and **pmemobj_xalloc** return 0. If *oidp*
is not NULL, the *PMEMoid* of the newly allocated object is stored in *oidp*.
If the allocation fails, -1 is returned and *errno* is set appropriately. If
the constructor returns a non-zero value, the allocation is canceled, -1 is
returned, and *errno* is set to **ECANCELED**. If *size* equals 0, or the
*flags* for **pmemobj_xalloc** are invalid, -1 is returned, *errno* is set
to **EINVAL**, and *oidp* is left untouched.

On success, **pmemobj_zalloc**() returns 0. If *oidp* is not NULL, the
*PMEMoid* of the newly allocated object is stored in *oidp*. If the allocation
fails, it returns -1 and sets *errno* appropriately. If *size* equals 0, it
returns -1, sets *errno* to **EINVAL**, and leaves *oidp* untouched.

The **pmemobj_free**() function returns no value.

On success, **pmemobj_realloc**() and **pmemobj_zrealloc**() return 0 and
update *oidp* if necessary. On error, they return -1 and set *errno*
appropriately.

On success, **pmemobj_strdup**() and **pmemobj_wcsdup**() return 0. If
*oidp* is not NULL, the *PMEMoid* of the duplicated string object is stored
in *oidp*. If *s* is NULL, they return -1, set *errno* to **EINVAL**, and
leave *oidp* untouched. On other errors, they return -1 and set *errno*
appropriately.

The **pmemobj_alloc_usable_size**() function returns the number of usable bytes
in the object represented by *oid*. If *oid* is **OID_NULL**, it returns 0.

On success, **pmemobj_defrag**() returns 0. If defragmentation was
unsuccessful or only partially successful (i.e. if it was aborted halfway
through due to lack of resources), -1 is returned.

# SEE ALSO #

**free**(3), **POBJ_FOREACH**(3), **realloc**(3),
**strdup**(3), **wcsdup**(3), **libpmemobj**(7)
and **<https://pmem.io>**

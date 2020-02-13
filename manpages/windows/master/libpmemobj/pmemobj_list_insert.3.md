---
layout: manual
Content-Style: 'text/css'
title: PMEMOBJ_LIST_INSERT
collection: libpmemobj
header: PMDK
date: pmemobj API version 2.3
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2017-2018, Intel Corporation)

[comment]: <> (pmemobj_list_insert.3 -- man page for non-transactional persistent atomic lists)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmemobj_list_insert**(), **pmemobj_list_insert_new**(),
**pmemobj_list_move**(), **pmemobj_list_remove**()
- non-transactional persistent atomic lists functions

# SYNOPSIS #

```c
#include <libpmemobj.h>

int pmemobj_list_insert(PMEMobjpool *pop, size_t pe_offset, void *head,
	PMEMoid dest, int before, PMEMoid oid);

PMEMoid pmemobj_list_insert_new(PMEMobjpool *pop, size_t pe_offset,
	void *head, PMEMoid dest, int before, size_t size,
	uint64_t type_num, pmemobj_constr constructor, void arg);

int pmemobj_list_move(PMEMobjpool *pop,
	size_t pe_old_offset, void *head_old,
	size_t pe_new_offset, void *head_new,
	PMEMoid dest, int before, PMEMoid oid);

int pmemobj_list_remove(PMEMobjpool *pop, size_t pe_offset,
	void *head, PMEMoid oid, int free);
```

# DESCRIPTION #

In addition to the container operations on internal object collections
described in **pmemobj_first**(3), **libpmemobj**(7) provides
a mechanism for organizing persistent objects in user-defined, persistent,
atomic, circular, doubly-linked lists. All the routines and macros operating
on the persistent lists provide atomicity with respect to any power-fail
interruptions. If any of those operations is torn by program failure or system
crash, on recovery they are guaranteed to be entirely completed or discarded,
leaving the lists, persistent memory heap and internal object containers in a
consistent state.

The persistent atomic circular doubly linked lists support the following functionality:

+ Insertion of an object at the head of the list, or at the end of the list.
+ Insertion of an object before or after any element in the list.
+ Atomic allocation and insertion of a new object at the head of the list, or at the end of the list.
+ Atomic allocation and insertion of a new object before or after any element in the list.
+ Atomic moving of an element from one list to the specific location on another list.
+ Removal of any object in the list.
+ Atomic removal and freeing of any object in the list.
+ Forward or backward traversal through the list.

A list is headed by a *list_head* structure containing the object handle of the
first element on the list. The elements are doubly linked so that an arbitrary
element can be removed without the need to traverse the list. New elements can
be added to the list before or after an existing element, at the head of the
list, or at the tail of the list. A list may be traversed in either direction.

The user-defined structure of each element must contain a field of type
*list_entry* that holds the object handles to the previous and next element
on the list. Both the *list_head* and the *list_entry* structures are
declared in **\<libpmemobj.h\>**.

The functions below are intended to be used outside transactions - transactional
variants are described in manpages to functions mentioned at **TRANSACTIONAL OBJECT
MANIPULATION** in **libpmemobj**(7). Note that operations performed using this
non-transactional API are independent from their transactional counterparts.
If any non-transactional allocations or list manipulations are performed within
an open transaction, the changes will not be rolled back if such a transaction
is aborted or interrupted.

The list insertion and move functions use a common set of arguments to define
where an object will be inserted into the list. *dest* identifies the element
before or after which the object will be inserted, or, if *dest* is
**OID_NULL**, indicates that the object should be inserted at the head or
tail of the list. *before* determines where the object will be inserted:

+ **POBJ_LIST_DEST_BEFORE** - insert the element before the existing
element *dest*

+ **POBJ_LIST_DEST_AFTER** - insert the element after the existing element
*dest*

+ **POBJ_LIST_DEST_HEAD** - when *dest* is **OID_NULL**, insert the element
at the head of the list

+ **POBJ_LIST_DEST_TAIL** - when *dest* is **OID_NULL**, insert the element
at the tail of the list

>NOTE: Earlier versions of **libpmemobj**(7) do not define
**POBJ_LIST_DEST_BEFORE** and **POBJ_LIST_DEST_AFTER**. Use 1 for before,
and 0 for after.

The **pmemobj_list_insert**() function inserts the element represented by
object handle *oid* into the list referenced by *head*, at the location
specified by *dest* and *before* as described above. *pe_offset*
specifies the offset of the structure that connects the elements in
the list. All the handles *head*, *dest* and *oid* must point to objects
allocated from memory pool *pop*. *head* and *oid* cannot be **OID_NULL**.

The **pmemobj_list_insert_new**() function atomically allocates a new object
of given *size* and type *type_num* and inserts it into the list referenced
by *head* at the location specified by *dest* and *before* as described
above. *pe_offset* specifies the offset of the structure that connects the
elements in the list. The handles *head* and *dest* must point to objects
allocated from memory pool *pop*. Before returning,
**pmemobj_list_insert_new**() calls the *constructor* function, passing the
pool handle *pop*, the pointer to the newly allocated object *ptr*, and the
*arg* argument. It is guaranteed that the allocated object is either properly
initialized or, if the allocation is interrupted before the constructor
completes, the memory space reserved for the object is reclaimed. *head*
cannot be **OID_NULL**. The allocated object is also added to the internal
container associated with *type_num*, as described in **POBJ_FOREACH**(3).

The **pmemobj_list_move**() function moves the object represented by object
handle *oid* from the list referenced by *head_old* to the list referenced
by *head_new*, inserting it at the location specified by *dest* and *before*
as described above. *pe_old_offset* and *pe_new_offset* specify the offsets
of the structures that connect the elements in the old and new lists,
respectively. All the handles *head_old*, *head_new*, *dest* and *oid* must
point to objects allocated from memory pool *pop*. *head_old*, *head_new*
and *oid* cannot be **OID_NULL**.

The **pmemobj_list_remove**() function removes the object represented by object
handle *oid* from the list referenced by *head*. If *free* is set, it also
removes the object from the internal object container and frees the associated
memory space. *pe_offset* specifies the offset of the structure that connects
the elements in the list. Both *head* and *oid* must point to objects allocated
from memory pool *pop* and cannot be **OID_NULL**.

# RETURN VALUE #

On success, **pmemobj_list_insert**(), **pmemobj_list_remove**() and
**pmemobj_list_move**() return 0. On error, they return -1 and set
*errno* appropriately.

On success, **pmemobj_list_insert_new**() returns a handle to the newly
allocated object. If the constructor returns a non-zero value, the allocation
is canceled, -1 is returned, and *errno* is set to **ECANCELED**.
On other errors, **OID_NULL** is returned and *errno* is set appropriately.

# SEE ALSO #

**pmemobj_first**(3), **POBJ_FOREACH**(3), **libpmemobj**(7)
and **<https://pmem.io>**

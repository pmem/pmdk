---
layout: manual
Content-Style: 'text/css'
title: PMEMOBJ_LIST_INSERT!3
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

[comment]: <> (pmemobj_list_insert.3 -- man page for non-transactional persistent atomic lists)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />


# NAME #

**pmemobj_list_insert**(), **pmemobj_list_insert_new**(),
**pmemobj_list_remove**(), **pmemobj_list_move**()
-- non-transactional persistent atomic lists functions


# SYNOPSIS #

```c
#include <libpmemobj.h>

int pmemobj_list_insert(PMEMobjpool *pop, size_t pe_offset, void *head,
	PMEMoid dest, int before, PMEMoid oid);

PMEMoid pmemobj_list_insert_new(PMEMobjpool *pop, size_t pe_offset,
	void *head, PMEMoid dest, int before, size_t size,
	uint64_t type_num, pmemobj_constr constructor, void arg);

int pmemobj_list_remove(PMEMobjpool *pop, size_t pe_offset,
	void *head, PMEMoid oid, int free);

int pmemobj_list_move(PMEMobjpool *pop,
	size_t pe_old_offset, void *head_old,
	size_t pe_new_offset, void *head_new,
	PMEMoid dest, int before, PMEMoid oid);
```


# DESCRIPTION #

Besides the internal objects collections mentioned at **libpmemobj**(7)
in section **OBJECT CONTAINERS** the **libpmemobj** provides a mechanism
to organize persistent objects in the user-defined persistent atomic circular
doubly linked lists. All the routines and macros operating on the persistent
lists provide atomicity with respect to any power-fail interruptions. If any
of those operations is torn by program failure or system crash; on recovery
they are guaranteed to be entirely completed or discarded, leaving the lists,
persistent memory heap and internal object containers in a consistent state.

The persistent atomic circular doubly linked lists support the following functionality:

+ Insertion of an object at the head of the list, or at the end of the list.
+ Insertion of an object before or after any element in the list.
+ Atomic allocation and insertion of a new object at the head of the list, or at the end of the list.
+ Atomic allocation and insertion of a new object before or after any element in the list.
+ Atomic moving of an element from one list to the specific location on another list.
+ Removal of any object in the list.
+ Atomic removal and freeing of any object in the list.
+ Forward or backward traversal through the list.

A list is headed by a *list_head* structure containing a single object handle of
the first element on the list. The elements are doubly linked so that an arbitrary
element can be removed without a need to traverse the list. New elements can be added
to the list before or after an existing element, at the head of the list,
or at the end of the list. A list may be traversed in either direction.

The user-defined structure of each element must contain a field of type
*list_entry* holding the object handles to the previous and next element
on the list. Both the *list_head* and the *list_entry* structures are
declared in **\<libpmemobj.h\>**.

The functions below are intended to be used outside transactions - transactional
variants are described in manpages to functions mentioned at **TRANSACTIONAL OBJECT
MANIPULATION** in **libpmemobj**(7). Note that operations performed using this
non-transactional API are independent from their transactional counterparts.
If any non-transactional allocations or list manipulations are performed within
an open transaction, the changes will not be rolled-back if such a transaction is
aborted or interrupted.


The **pmemobj_list_insert**() function inserts an element represented by object
handle *oid* into the list referenced by *head*. Depending on the value of flag
*before*, the object is added before or after the element *dest*. If *dest* value
is **OID_NULL**, the object is inserted at the head or at the end of the list,
depending on the *before* flag value. If value is non-zero the object is inserted
at the head, if value is zero the object is inserted at the end of the list. The
relevant values are available through **POBJ_LIST_DEST_HEAD** and
**POBJ_LIST_DEST_TAIL** defines respectively. The argument *pe_offset* declares an
offset of the structure that connects the elements in the list. All the handles
*head*, *dest* and *oid* must point to the objects allocated from the same memory
pool *pop*. The *head* and *oid* cannot be **OID_NULL**.

The **pmemobj_list_insert_new**() function atomically allocates a new object of
given *size* and type *type_num* and inserts it into the list referenced by *head*.
Depending on the value of *before* flag, the newly allocated object is added
before or after the element *dest*. If *dest* value is **OID_NULL**, the object
is inserted at the head or at the end of the list, depending on the *before* flag
value. If value is non-zero the object is inserted at the head, if value is zero
the object is inserted at the end of the list. The relevant values are available
through **POBJ_LIST_DEST_HEAD** and **POBJ_LIST_DEST_TAIL** defines respectively.
The argument *pe_offset* declares an offset of the structure that connects the
elements in the list. All the handles *head*, *dest* must point to the objects
allocated from the same memory pool *pop*. Before returning, it calls the
*constructor* function passing the pool handle *pop*, the pointer to the newly
allocated object in *ptr* along with the *arg* argument. It is guaranteed that
allocated object is either properly initialized or, if the allocation is interrupted
before the constructor completes, the memory space reserved for the object is
reclaimed. The *head* cannot be **OID_NULL**. The allocated object is also added
to the internal container associated with given *type_num*. as described in manpage
**POBJ_FOREACH**(3).

The **pmemobj_list_remove**() function removes the object referenced by *oid* from
the list pointed by *head*. If *free* flag is set, it also removes the object from
the internal object container and frees the associated memory space. The argument
*pe_offset* declares an offset of the structure that connects the elements in the list.
Both *head* and *oid* must point to the objects allocated from the same memory pool
*pop* and cannot be **OID_NULL**.

```c
int pmemobj_list_move(PMEMobjpool *pop,
	size_t pe_old_offset, void *head_old,
	size_t pe_new_offset, void *head_new,
	PMEMoid dest, int before, PMEMoid oid);
```

The **pmemobj_list_move**() function moves the object represented by *oid* from
the list pointed by *head_old* to the list pointed by *head_new*. Depending on
the value of flag *before*, the newly allocated object is added before or after
the element *dest*. If *dest* value is **OID_NULL**, the object is inserted at
the head or at the end of the second list, depending on the *before* flag value.
If value is non-zero the object is inserted at the head, if value is zero the
object is inserted at the end of the list. The relevant values are available
through **POBJ_LIST_DEST_HEAD** and **POBJ_LIST_DEST_TAIL** defines respectively.
The arguments *pe_old_offset* and *pe_new_offset* declare the offsets of the
structures that connects the elements in the old and new lists respectively.
All the handles *head_old*, *head_new*, *dest* and *oid* must point to the objects
allocated from the same memory pool *pop*. *head_old*, *head_new* and *oid* cannot
be **OID_NULL**.


# RETURN VALUE #

The **pmemobj_list_insert**() function on success returns zero.
On error, -1 is returned and *errno* is set.

The **pmemobj_list_insert_new**() function on success returns a handle to the
newly allocated object. On error, **OID_NULL** is returned and *errno* is set.
If the constructor returns non-zero value the allocation is canceled,
the -1 value is returned from the caller and *errno* is set to **ECANCELED**.

The **pmemobj_list_remove**() and **pmemobj_list_move**() function on success
returns zero. On error, -1 is returned and *errno* is set.


# SEE ALSO #

**POBJ_FOREACH**(3), **libpmemobj**(7) and **<http://pmem.io>**

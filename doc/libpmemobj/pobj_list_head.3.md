---
layout: manual
Content-Style: 'text/css'
title: _MP(POBJ_LIST_HEAD, 3)
collection: libpmemobj
header: PMDK
date: pmemobj API version 2.3
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2017-2018, Intel Corporation)

[comment]: <> (pobj_list_head.3 -- man page for type-safe non-transactional persistent atomic lists)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**POBJ_LIST_HEAD**(),
**POBJ_LIST_ENTRY**(),
**POBJ_LIST_FIRST**(),
**POBJ_LIST_LAST**(),
**POBJ_LIST_EMPTY**(),
**POBJ_LIST_NEXT**(),
**POBJ_LIST_PREV**(),

**POBJ_LIST_FOREACH**(),
**POBJ_LIST_FOREACH_REVERSE**(),

**POBJ_LIST_INSERT_HEAD**(),
**POBJ_LIST_INSERT_TAIL**(),
**POBJ_LIST_INSERT_AFTER**(),
**POBJ_LIST_INSERT_BEFORE**(),
**POBJ_LIST_INSERT_NEW_HEAD**(),
**POBJ_LIST_INSERT_NEW_TAIL**(),
**POBJ_LIST_INSERT_NEW_AFTER**(),
**POBJ_LIST_INSERT_NEW_BEFORE**(),

**POBJ_LIST_REMOVE**(),
**POBJ_LIST_REMOVE_FREE**(),

**POBJ_LIST_MOVE_ELEMENT_HEAD**(),
**POBJ_LIST_MOVE_ELEMENT_TAIL**(),
**POBJ_LIST_MOVE_ELEMENT_AFTER**(),
**POBJ_LIST_MOVE_ELEMENT_BEFORE**()
- type-safe non-transactional persistent atomic lists

# SYNOPSIS #

```c
#include <libpmemobj.h>

POBJ_LIST_HEAD(HEADNAME, TYPE)
POBJ_LIST_ENTRY(TYPE)
POBJ_LIST_FIRST(POBJ_LIST_HEAD *head)
POBJ_LIST_LAST(POBJ_LIST_HEAD *head, POBJ_LIST_ENTRY FIELD)
POBJ_LIST_EMPTY(POBJ_LIST_HEAD *head)
POBJ_LIST_NEXT(TOID elm, POBJ_LIST_ENTRY FIELD)
POBJ_LIST_PREV(TOID elm, POBJ_LIST_ENTRY FIELD)

POBJ_LIST_FOREACH(TOID var, POBJ_LIST_HEAD *head, POBJ_LIST_ENTRY FIELD)
POBJ_LIST_FOREACH_REVERSE(TOID var, POBJ_LIST_HEAD *head, POBJ_LIST_ENTRY FIELD)

POBJ_LIST_INSERT_HEAD(PMEMobjpool *pop, POBJ_LIST_HEAD *head,
	TOID elm, POBJ_LIST_ENTRY FIELD)
POBJ_LIST_INSERT_TAIL(PMEMobjpool *pop, POBJ_LIST_HEAD *head,
	TOID elm, POBJ_LIST_ENTRY FIELD)
POBJ_LIST_INSERT_AFTER(PMEMobjpool *pop, POBJ_LIST_HEAD *head,
	TOID listelm, TOID elm, POBJ_LIST_ENTRY FIELD)
POBJ_LIST_INSERT_BEFORE(PMEMobjpool *pop, POBJ_LIST_HEAD *head,
	TOID listelm, TOID elm, POBJ_LIST_ENTRY FIELD)
POBJ_LIST_INSERT_NEW_HEAD(PMEMobjpool *pop, POBJ_LIST_HEAD *head,
	POBJ_LIST_ENTRY FIELD, size_t size,
	pmemobj_constr constructor, void *arg)
POBJ_LIST_INSERT_NEW_TAIL(PMEMobjpool *pop, POBJ_LIST_HEAD *head,
	POBJ_LIST_ENTRY FIELD, size_t size,
	pmemobj_constr constructor, void *arg)
POBJ_LIST_INSERT_NEW_AFTER(PMEMobjpool *pop, POBJ_LIST_HEAD *head,
	TOID listelm, POBJ_LIST_ENTRY FIELD, size_t size,
	pmemobj_constr constructor, void *arg)
POBJ_LIST_INSERT_NEW_BEFORE(PMEMobjpool *pop, POBJ_LIST_HEAD *head,
	TOID listelm, POBJ_LIST_ENTRY FIELD, size_t size,
	pmemobj_constr constructor, void *arg)

POBJ_LIST_REMOVE(PMEMobjpool *pop, POBJ_LIST_HEAD *head,
	TOID elm, POBJ_LIST_ENTRY FIELD)
POBJ_LIST_REMOVE_FREE(PMEMobjpool *pop, POBJ_LIST_HEAD *head,
	TOID elm, POBJ_LIST_ENTRY FIELD)

POBJ_LIST_MOVE_ELEMENT_HEAD(PMEMobjpool *pop, POBJ_LIST_HEAD *head,
	POBJ_LIST_HEAD *head_new, TOID elm, POBJ_LIST_ENTRY FIELD,
	POBJ_LIST_ENTRY field_new)
POBJ_LIST_MOVE_ELEMENT_TAIL(PMEMobjpool *pop, POBJ_LIST_HEAD *head,
	POBJ_LIST_HEAD *head_new, TOID elm, POBJ_LIST_ENTRY FIELD,
	POBJ_LIST_ENTRY field_new)
POBJ_LIST_MOVE_ELEMENT_AFTER(PMEMobjpool *pop, POBJ_LIST_HEAD *head,
	POBJ_LIST_HEAD *head_new, TOID listelm, TOID elm,
	POBJ_LIST_ENTRY FIELD, POBJ_LIST_ENTRY field_new)
POBJ_LIST_MOVE_ELEMENT_BEFORE(PMEMobjpool *pop, POBJ_LIST_HEAD *head,
	POBJ_LIST_HEAD *head_new, TOID listelm, TOID elm,
	POBJ_LIST_ENTRY FIELD, POBJ_LIST_ENTRY field_new)
```

# DESCRIPTION #

The following macros define and operate on a type-safe persistent
atomic circular doubly linked list data structure that consist of
a set of persistent objects of a well-known type. Unlike the functions
described in the previous section, these macros provide type enforcement
by requiring declaration of type of the objects stored in given list,
and not allowing mixing objects of different types in a single list.

The functionality and semantics of those macros is similar to circular
queues defined in **queue**(3).

The majority of the macros must specify the handle to the memory pool *pop* and
the name of the *field* in the user-defined structure, which must be of type
*POBJ_LIST_ENTRY* and is used to connect the elements in the list.

A list is headed by a structure defined by the **POBJ_LIST_HEAD**() macro.
This structure contains an object handle of the first element on the list.
The elements are doubly linked so that an arbitrary element can be removed
without a need to traverse the list. New elements can be added to the list
before or after an existing element, at the head of the list, or at the end
of the list. A list may be traversed in either direction. A *POBJ_LIST_HEAD*
structure is declared as follows:

```c
#define POBJ_LIST_HEAD(HEADNAME, TYPE)
struct HEADNAME
{
	TOID(TYPE) pe_first;
	PMEMmutex lock;
};
```

In the macro definitions, *TYPE* is the name of a user-defined structure,
that must contain a field of type *POBJ_LIST_ENTRY*. The argument *HEADNAME*
is the name of a user-defined structure that must be declared using the macro
*POBJ_LIST_HEAD*. See the examples below for further explanation of how these
macros are used.

The macro *POBJ_LIST_ENTRY* declares a structure that connects the elements
in the list.

```c
#define POBJ_LIST_ENTRY(TYPE)
struct
{
	TOID(TYPE) pe_next;
	TOID(TYPE) pe_prev;
};
```

The macro **POBJ_LIST_FIRST**() returns the first element on the list
referenced by *head*. If the list is empty **OID_NULL** is returned.

The macro **POBJ_LIST_LAST**() returns the last element on the list
referenced by *head*. If the list is empty **OID_NULL** is returned.

The macro **POBJ_LIST_EMPTY**() evaluates to 1 if the list referenced
by *head* is empty. Otherwise, 0 is returned.

The macro **POBJ_LIST_NEXT**() returns the element next to the element *elm*.

The macro **POBJ_LIST_PREV**() returns the element preceding the element *elm*.

The macro **POBJ_LIST_FOREACH**() traverses the list referenced by *head*
assigning a handle to each element in turn to *var* variable.

The macro **POBJ_LIST_FOREACH_REVERSE**() traverses the list referenced
by *head* in reverse order, assigning a handle to each element in turn to
*var* variable. The *field* argument is the name of the field of type
*POBJ_LIST_ENTRY* in the element structure.

The macro **POBJ_LIST_INSERT_HEAD**() inserts the element *elm* at the head
of the list referenced by *head*.

The macro **POBJ_LIST_INSERT_TAIL**() inserts the element *elm* at the end
of the list referenced by *head*.

The macro **POBJ_LIST_INSERT_AFTER**() inserts the element *elm* into
the list referenced by *head* after the element *listelm*. If *listelm*
value is **TOID_NULL**, the object is inserted at the end of the list.

The macro **POBJ_LIST_INSERT_BEFORE**() inserts the element *elm* into
the list referenced by *head* before the element *listelm*. If *listelm*
value is **TOID_NULL**, the object is inserted at the head of the list.

The macro **POBJ_LIST_INSERT_NEW_HEAD**() atomically allocates a new
object of size *size* and inserts it at the head of the list referenced
by *head*. The newly allocated object is also added to the internal object
container associated with a type number which is retrieved from the typed
*OID* of the first element on list.

The macro **POBJ_LIST_INSERT_NEW_TAIL**() atomically allocates a new
object of size *size* and inserts it at the tail of the list referenced
by *head*. The newly allocated object is also added to the internal object
container associated with a type number which is retrieved from
the typed *OID* of the first element on list.

The macro **POBJ_LIST_INSERT_NEW_AFTER**() atomically allocates a new
object of size *size* and inserts it into the list referenced by *head*
after the element *listelm*. If *listelm* value is **TOID_NULL**,
the object is inserted at the end of the list. The newly allocated object
is also added to the internal object container associated with with a type
number which is retrieved from the typed *OID* of the first element on list.

The macro **POBJ_LIST_INSERT_NEW_BEFORE**() atomically allocates a new
object of size *size* and inserts it into the list referenced by *head*
before the element *listelm*. If *listelm* value is **TOID_NULL**,
the object is inserted at the head of the list. The newly allocated object
is also added to the internal object container associated with with a type
number which is retrieved from the typed *OID* of the first element on list.

The macro **POBJ_LIST_REMOVE**() removes the element *elm* from the list
referenced by *head*.

The macro **POBJ_LIST_REMOVE_FREE**() removes the element *elm* from the list
referenced by *head* and frees the memory space represented by this element.

The macro **POBJ_LIST_MOVE_ELEMENT_HEAD**() moves the element *elm* from the
list referenced by *head* to the head of the list *head_new*. The *field* and
*field_new* arguments are the names of the fields of type *POBJ_LIST_ENTRY* in
the element structure that are used to connect the elements in both lists.

The macro **POBJ_LIST_MOVE_ELEMENT_TAIL**() moves the element *elm* from the
list referenced by *head* to the end of the list *head_new*. The *field* and
*field_new* arguments are the names of the fields of type *POBJ_LIST_ENTRY* in
the element structure that are used to connect the elements in both lists.

The macro **POBJ_LIST_MOVE_ELEMENT_AFTER**() atomically removes the element *elm*
from the list referenced by *head* and inserts it into the list referenced by
*head_new* after the element *listelm*. If *listelm* value is *TOID_NULL*,
the object is inserted at the end of the list. The *field* and *field_new*
arguments are the names of the fields of type *POBJ_LIST_ENTRY* in the element
structure that are used to connect the elements in both lists.

The macro **POBJ_LIST_MOVE_ELEMENT_BEFORE**() atomically removes the element
*elm* from the list referenced by *head* and inserts it into the list referenced
by *head_new* before the element *listelm*. If *listelm* value is **TOID_NULL**,
the object is inserted at the head of the list. The *field* and *field_new*
arguments are the names of the fields of type *POBJ_LIST_ENTRY* in the element
structure that are used to connect the elements in both lists.

# SEE ALSO #

**queue**(3), **libpmemobj**(7) and **<https://pmem.io>**

---
layout: manual
Content-Style: 'text/css'
title: _MP(PMEMOBJ_FIRST, 3)
collection: libpmemobj
header: PMDK
date: pmemobj API version 2.3
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2017-2018, Intel Corporation)

[comment]: <> (pmemobj_first.3 -- man page for pmemobj container operations)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmemobj_first**(), **pmemobj_next**(),
**POBJ_FIRST**(), **POBJ_FIRST_TYPE_NUM**(),
**POBJ_NEXT**(), **POBJ_NEXT_TYPE_NUM**(),
**POBJ_FOREACH**(), **POBJ_FOREACH_SAFE**(),
**POBJ_FOREACH_TYPE**(), **POBJ_FOREACH_SAFE_TYPE**()
- pmemobj container operations

# SYNOPSIS #

```c
#include <libpmemobj.h>

PMEMoid pmemobj_first(PMEMobjpool *pop);
PMEMoid pmemobj_next(PMEMoid oid);

POBJ_FIRST(PMEMobjpool *pop, TYPE)
POBJ_FIRST_TYPE_NUM(PMEMobjpool *pop, uint64_t type_num)
POBJ_NEXT(TOID oid)
POBJ_NEXT_TYPE_NUM(PMEMoid oid)

POBJ_FOREACH(PMEMobjpool *pop, PMEMoid varoid)
POBJ_FOREACH_SAFE(PMEMobjpool *pop, PMEMoid varoid, PMEMoid nvaroid)
POBJ_FOREACH_TYPE(PMEMobjpool *pop, TOID var)
POBJ_FOREACH_SAFE_TYPE(PMEMobjpool *pop, TOID var, TOID nvar)
```

# DESCRIPTION #

The **libpmemobj**(7) container operations provide a mechanism that allows
iteration through the internal object collection, either looking for a
specific object, or performing a specific operation on each object of a given
type. Software should not make any assumptions about the order of the objects
in the internal object containers.

The **pmemobj_first**() function returns the first object from the pool.

The **POBJ_FIRST**() macro returns the first object from the pool of
the type specified by *TYPE*.

The **POBJ_FIRST_TYPE_NUM**() macro returns the first object from the pool
of the type specified by *type_num*.

The **pmemobj_next**() function returns the next object from the pool.

The **POBJ_NEXT**() macro returns the next object of the same type
as the object referenced by *oid*.

The **POBJ_NEXT_TYPE_NUM**() macro returns the next object of the same type
number as the object referenced by *oid*.

The following four macros provide a more convenient way to iterate through the
internal collections, performing a specific operation on each object.

The **POBJ_FOREACH**() macro performs a specific operation on each allocated
object stored in the persistent memory pool *pop*. It traverses the internal
collection of all the objects, assigning a handle to each element in turn to
*varoid*.

The **POBJ_FOREACH_TYPE**() macro performs a specific operation on each
allocated object stored in the persistent memory pool *pop* that has the same
type as *var*. It traverses the internal collection of all the objects of the
specified type, assigning a handle to each element in turn to *var*.

The macros **POBJ_FOREACH_SAFE**() and **POBJ_FOREACH_SAFE_TYPE**() work in a
similar fashion as **POBJ_FOREACH**() and **POBJ_FOREACH_TYPE**(), except that
prior to performing the operation on the object, they preserve a handle to the
next object in the collection by assigning it to *nvaroid* or *nvar*,
respectively. This allows safe deletion of selected objects while iterating
through the collection.

# RETURN VALUE #

**pmemobj_first**() returns the first object from the pool, or, if the pool
is empty, **OID_NULL**.

**pmemobj_next**() returns the next object from the pool. If the object
referenced by *oid* is the last object in the collection, or if *oid*
is *OID_NULL*, **pmemobj_next**() returns **OID_NULL**.

# SEE ALSO #

**libpmemobj**(7) and **<https://pmem.io>**

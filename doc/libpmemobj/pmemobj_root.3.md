---
layout: manual
Content-Style: 'text/css'
title: _MP(PMEMOBJ_ROOT, 3)
collection: libpmemobj
header: PMDK
date: pmemobj API version 2.3
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2017-2018, Intel Corporation)

[comment]: <> (pmemobj_root.3 -- man page for root object management)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmemobj_root**(), **pmemobj_root_construct**()
**POBJ_ROOT**(), **pmemobj_root_size**() - root object management

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

The root object of a persistent memory pool is an entry point for all other
persistent objects allocated using the **libpmemobj** API. In other words,
every object stored in the persistent memory pool has the root
object at the end of its reference path. It may be assumed that for each
persistent memory pool the root object always exists, and there is exactly
one root object in each pool.

The **pmemobj_root**() function creates or resizes the root object for the
persistent memory pool *pop*. If this is the first call to **pmemobj_root**(),
the requested *size* is greater than zero and the root object does not exist,
it is implicitly allocated
in a thread-safe manner, so the function may be called by more than one
thread simultaneously (as long as all threads use the identical *size* value).
The size of the root object is guaranteed to be not less than the requested
*size*. If the requested size is larger than the current size, the root
object is automatically resized. In such case, the old data is preserved and
the extra space is zeroed. If the requested size is equal to or smaller than
the current size, the root object remains unchanged.
If the requested *size* is equal to zero, the root object is not allocated.

**pmemobj_root_construct**() performs the same actions as **pmemobj_root**(),
but instead of zeroing the newly allocated object a *constructor* function
is called to initialize the object. The constructor is also called on
reallocations.

The **POBJ_ROOT**() macro works the same way as the **pmemobj_root**() function
except it returns a typed *OID* value.

The **pmemobj_root_size**() function returns the current size of the root object
associated with the persistent memory pool *pop*.

# RETURN VALUE #

Upon success, **pmemobj_root**() returns a handle to the root object associated
with the persistent memory pool *pop*. The same root object handle is returned
in all the threads. If the requested object size is larger than the maximum
allocation size supported for the pool, or if there is not enough free
space in the pool to satisfy a reallocation request, **pmemobj_root**() returns
**OID_NULL** and sets *errno* to ENOMEM.
If the *size* was equal to zero and the root object has not been allocated,
**pmemobj_root**() returns **OID_NULL** and sets *errno* to EINVAL.

If the **pmemobj_root_construct**() constructor fails, the allocation is
canceled, **pmemobj_root_construct**() returns *OID_NULL*, and *errno* is set
to **ECANCELED**. **pmemobj_root_size**() can be used in the constructor to
check whether this is the first call to the constructor.

**POBJ_ROOT**() returns a typed *OID* of type *TYPE* instead of the
*PMEMoid* returned by **pmemobj_root**().

The **pmemobj_root_size**() function returns the current size of the root object
associated with the persistent memory pool *pop*. The returned size is the
largest value requested by any of the earlier **pmemobj_root**() calls. If the
root object has not been allocated yet, **pmemobj_root_size**() returns 0.

# SEE ALSO #

**libpmemobj**(7) and **<https://pmem.io>**

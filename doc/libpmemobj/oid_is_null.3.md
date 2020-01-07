---
layout: manual
Content-Style: 'text/css'
title: _MP(OID_IS_NULL, 3)
collection: libpmemobj
header: PMDK
date: pmemobj API version 2.3
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2017-2018, Intel Corporation)

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
**pmemobj_pool_by_ptr**() - functions that allow mapping
operations between object addresses, object handles, oids or type numbers

# SYNOPSIS #

```c
#include <libpmemobj.h>

OID_IS_NULL(PMEMoid oid)
OID_EQUALS(PMEMoid lhs, PMEMoid rhs)

void *pmemobj_direct(PMEMoid oid);
PMEMoid pmemobj_oid(const void *addr);
uint64_t pmemobj_type_num(PMEMoid oid);
PMEMobjpool *pmemobj_pool_by_oid(PMEMoid oid);
PMEMobjpool *pmemobj_pool_by_ptr(const void *addr);
void *pmemobj_volatile(PMEMobjpool *pop, struct pmemvlt *vlt,
	size_t size, void *ptr,
	int (*constr)(void *ptr, void *arg), void *arg); (EXPERIMENTAL)
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

For special cases where volatile (transient) variables need to be stored on
persistent memory, there's a mechanism composed of *struct pmemvlt* type and
**pmemobj_volatile()** function. To use it, the *struct pmemvlt* needs to
be placed in the neighborhood of transient data region. The *PMEMvlt* macro
can be used to construct such a region.
The *struct pmemvlt* must be zeroed prior to use. This can be easily done in
object constructor or in a transaction directly after an allocation.
When the **pmemobj_volatile()** function is called on a *struct pmemvlt*,
it will return the pointer to the data and it will ensure that the provided
constructor function is called exactly once in the current instance of the
pmemobj pool.
The constructor is called with the *ptr* pointer to the data, and this function
will return the same pointer if the constructor returns *0*, otherwise NULL is
returned. The *size* argument must accurately describe the total size of the
volatile memory region that will be accessed. Calling **pmemobj_volatile()**
on the same region with different sizes is undefined behavior.
For this mechanism to be effective, all accesses to transient variables must
go through it, otherwise there's a risk of the constructor not being called
on the first load.
Maintaining transient state on persistent memory is challenging due to
difficulties with dynamic resources acquisition and subsequent resource release.
For example, one needs to consider what happens with volatile state of an object
which is being freed inside of a transaction, especially with regards to the
possibility of an abort.
It's generally recommended to entirely separate the persistent and transient
states, and when it's not possible, to only store types which do not require
lifecycle management (i.e., primitive types) inside of volatile regions.

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
contains the object represented by *oid*. If the pool is not open or
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

# EXAMPLES #

The following code shows how to store transient variables on persistent memory.

```c
struct my_data {
	PMEMvlt(uint64_t) foo;
	uint64_t bar;
};

int
my_data_constructor(void *ptr, void *arg)
{
	uint64_t *foo = ptr;
	*foo = 0;

	return 0;
}

PMEMobjpool *pop = ...;

struct my_data *data = D_RW(...);

uint64_t *foo = pmemobj_volatile(pop, &data->foo.vlt, &data->foo.value,
	my_data_constructor, NULL);

assert(*foo == 0);
```

# SEE ALSO #

**libpmemobj**(7) and **<https://pmem.io>**

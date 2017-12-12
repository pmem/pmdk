---
layout: manual
Content-Style: 'text/css'
title: _MP(PMEMOBJ_ACTION, 3)
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

[comment]: <> (pmemobj_action.3 -- Delayed atomicity actions)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[EXAMPLES](#examples)<br />
[SEE ALSO](#see-also)<br />


# NAME #

**pmemobj_reserve**(), **pmemobj_set_value**(), **pmemobj_publish**(),
**pmemobj_tx_publish**(), **pmemobj_cancel**(),
**POBJ_RESERVE_NEW**(), **POBJ_RESERVE_ALLOC**()
-- Delayed atomicity actions


# SYNOPSIS #

```c
#include <libpmemobj.h>

PMEMoid pmemobj_reserve(PMEMobjpool *pop, struct pobj_action *act,
	size_t size, uint64_t type_num);
void pmemobj_set_value(PMEMobjpool *pop, struct pobj_action *act,
	uint64_t *ptr, uint64_t value);
void pmemobj_publish(PMEMobjpool *pop, struct pobj_action *actv, int actvcnt);
int pmemobj_tx_publish(struct pobj_action *actv, int actvcnt);
pmemobj_cancel(PMEMobjpool *pop, struct pobj_action *actv, int actvcnt);

POBJ_RESERVE_NEW(pop, t, act)
POBJ_RESERVE_ALLOC(pop, t, size, act)
```

# DESCRIPTION #

All of the functions described so far have an immediate effect on the persistent
state of the pool, and as such, the cost of maintaining fail-safety is paid
outright and, most importantly, in the calling thread. This behavior makes
implementing algorithms involving relaxed consistency guarantees difficult, if
not outright impossible.

The following set of functions introduce a mechanism that allows one to delay
the persistent publication of a set of prepared actions to an arbitrary moment
in time of the execution of a program.

The publication is fail-safe atomic in the scope of the entire collection of
actions, but the number of said actions is limited by *POBJ_MAX_ACTIONS*
constant. If a program exists without publishing the actions, or the actions are
canceled, any resources reserved by those actions are released and placed back in
the pool.

A single action is represented by a single `struct pobj_action`. Functions that
create actions take that structure by pointer, whereas functions that publish
actions take array of actions and the size of the array. The actions can be
created, and published, from different threads.
When creating actions, the *act* argument must be non-NULL and point to a
`struct pobj_action`, the structure will be populated by the function and must
not be modified or deallocated until after publishing.

The **pmemobj_reserve**() functions performs a transient reservation of an object.
Behaves similarly to **pmemobj_alloc**(3), but performs no modification to the
persistent state.
The object returned by this function can be freely modified without worrying
about fail-safe atomicity until the object has been published. Any modifications
of the object must be manually persisted, just like in the case of the atomic API.

The **pmemobj_set_value** function prepares an action that, once published, will
modify the memory location pointed to by *ptr* to *value*.

The **pmemobj_publish** function publishes the provided set of actions. The
publication is fail-safe atomic. Once done, the persistent state will reflect
the changes contained in the actions.
The *actvcnt* cannot exceed *POBJ_MAX_ACTIONS*.

The **pmemobj_tx_publish** function moves the provided actions to the scope of
the transaction in which it is called. Only object reservations are supported
in transactional publish. Once done, the reserved objects will follow normal
transactional semantics. Can only be called during *TX_STAGE_WORK*.

The **pmemobj_cancel** function releases any resources held by the provided
set of actions and invalidates all actions.

The **POBJ_RESERVE_NEW** macro is a typed variant of **pmemobj_reserve**.
The size of the reservation is determined from the provided type *t*.

The **POBJ_RESERVE_ALLOC** macro is a typed variant of **pmemobj_reserve**.
The *size* of the reservation is user-provided.

# EXAMPLES #

The following code shows atomic append of two objects into a singly linked list.

```c
struct list_node {
	int value;
	PMEMoid next;
};
/* statically allocate the array of actions */
struct pobj_action actv[4];

/* reserve, populate and persist the first object */
PMEMoid tail = pmemobj_reserve(pop, &actv[0], sizeof(struct list_node), 0);
if (TOID_IS_NULL(tail))
	return -1;
D_RW(tail)->value = 1;
D_RW(tail)->next = OID_NULL;
pmemobj_persist(pop, D_RW(tail), sizeof(struct list_node));

/* reserve, populate and persist the second object */
PMEMoid head = pmemobj_reserve(pop, &actv[1], sizeof(struct list_node), 0);
if (TOID_IS_NULL(head))
	return -1;
D_RW(head)->value = 2;
D_RW(head)->next = tail;
pmemobj_persist(pop, D_RW(head), sizeof(struct list_node));

/* create actions to set the PMEMoid to the new values */
pmemobj_set_value(pop, &actv[2], &D_RO(root)->head.pool_uuid_lo, head.pool_uuid_lo);
pmemobj_set_value(pop, &actv[3], &D_RO(root)->head.off, head.off);

/* atomically publish the above actions */
pmemobj_publish(pop, actv, 4);
```

# RETURN VALUE #

On success, **pmemobj_reserve**() functions return a handle to the newly
reserved object, otherwise an *OID_NULL* is returned.

On success, **pmemobj_tx_publish**() returns 0, otherwise,
stage changes to *TX_STAGE_ONABORT* and *errno* is set appropriately

# SEE ALSO #

**pmemobj_alloc**(3), **pmemobj_tx_alloc**(3), **libpmemobj**(7)
and **<http://pmem.io>**

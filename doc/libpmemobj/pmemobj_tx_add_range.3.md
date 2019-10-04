---
layout: manual
Content-Style: 'text/css'
title: _MP(PMEMOBJ_TX_ADD_RANGE, 3)
collection: libpmemobj
header: PMDK
date: pmemobj API version 2.3
...

[comment]: <> (Copyright 2017-2018, Intel Corporation)

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

[comment]: <> (pmemobj_tx_add_range.3 -- man page for transactional object manipulation)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />


# NAME #

**pmemobj_tx_add_range**(), **pmemobj_tx_add_range_direct**(),
**pmemobj_tx_xadd_range**(), **pmemobj_tx_xadd_range_direct**()

**TX_ADD**(), **TX_ADD_FIELD**(),
**TX_ADD_DIRECT**(), **TX_ADD_FIELD_DIRECT**(),

**TX_XADD**(), **TX_XADD_FIELD**(),
**TX_XADD_DIRECT**(), **TX_XADD_FIELD_DIRECT**(),

**TX_SET**(), **TX_SET_DIRECT**(),
**TX_MEMCPY**(), **TX_MEMSET**()
- transactional object manipulation


# SYNOPSIS #

```c
#include <libpmemobj.h>

int pmemobj_tx_add_range(PMEMoid oid, uint64_t off, size_t size);
int pmemobj_tx_add_range_direct(const void *ptr, size_t size);
int pmemobj_tx_xadd_range(PMEMoid oid, uint64_t off, size_t size, uint64_t flags);
int pmemobj_tx_xadd_range_direct(const void *ptr, size_t size, uint64_t flags);

TX_ADD(TOID o)
TX_ADD_FIELD(TOID o, FIELD)
TX_ADD_DIRECT(TYPE *p)
TX_ADD_FIELD_DIRECT(TYPE *p, FIELD)

TX_XADD(TOID o, uint64_t flags)
TX_XADD_FIELD(TOID o, FIELD, uint64_t flags)
TX_XADD_DIRECT(TYPE *p, uint64_t flags)
TX_XADD_FIELD_DIRECT(TYPE *p, FIELD, uint64_t flags)

TX_SET(TOID o, FIELD, VALUE)
TX_SET_DIRECT(TYPE *p, FIELD, VALUE)
TX_MEMCPY(void *dest, const void *src, size_t num)
TX_MEMSET(void *dest, int c, size_t num)
```


# DESCRIPTION #

**pmemobj_tx_add_range**() takes a "snapshot" of the memory block of given
*size*, located at given offset *off* in the object specified by *oid*, and
saves it to the undo log. The application is then free to directly modify the
object in that memory range. In case of a failure or abort, all the changes
within this range will be rolled back. The supplied block of memory has to be
within the pool registered in the transaction. This function must be called
during **TX_STAGE_WORK**.

The **pmemobj_tx_xadd_range**() function behaves exactly the same as
**pmemobj_tx_add_range**() when *flags* equals zero.
*flags* is a bitmask of the following values:

+ **POBJ_XADD_NO_FLUSH** - skip flush on commit (when application deals
with flushing or uses pmemobj_memcpy_persist)

+ **POBJ_XADD_NO_SNAPSHOT** - added range will not be "snapshotted", i.e. any
changes made within it during the transaction will not be rolled backed after
abort

+ **POBJ_XADD_ASSUME_INITIALIZED** - added range is assumed to be initialized.
If this flag is not specified, passing uninitialized memory will result in an
error when run under Valgrind memcheck.

**pmemobj_tx_add_range_direct**() behaves the same as
**pmemobj_tx_add_range**() with the exception that it operates on virtual
memory addresses and not persistent memory objects. It takes a "snapshot" of
a persistent memory block of given *size*, located at the given address *ptr*
in the virtual memory space and saves it to the undo log. The application is
then free to directly modify the object in that memory range. In case of a
failure or abort, all the changes within this range will be rolled back.
The supplied block of memory has to be within the pool registered in the
transaction. This function must be called during **TX_STAGE_WORK**.

The **pmemobj_tx_xadd_range_direct**() function behaves exactly the same as
**pmemobj_tx_add_range_direct**() when *flags* equals zero. *flags* is a
bitmask of the following values:

+ **POBJ_XADD_NO_FLUSH** - skip flush on commit (when application deals
with flushing or uses pmemobj_memcpy_persist)

+ **POBJ_XADD_NO_SNAPSHOT** - added range will not be "snapshotted", i.e. any
changes made within it during the transaction will not be rolled backed after
abort

+ **POBJ_XADD_ASSUME_INITIALIZED** - added range is assumed to be initialized.
If this flag is not specified, passing uninitialized memory will result in an
error when run under Valgrind memcheck.

Similarly to the macros controlling the transaction flow, **libpmemobj**
defines a set of macros that simplify the transactional operations on
persistent objects. Note that those macros operate on typed object handles,
thus eliminating the need to specify the size of the object, or the size and
offset of the field in the user-defined structure that is to be modified.

The **TX_ADD_FIELD**() macro saves the current value of given *FIELD* of the
object referenced by a handle *o* in the undo log. The application is then free
to directly modify the specified *FIELD*. In case of a failure or abort, the
saved value will be restored.

The **TX_XADD_FIELD**() macro works exactly like **TX_ADD_FIELD** when *flags*
equals 0. The *flags* argument is a bitmask of values described in
**pmemobj_tx_xadd_range**, above.

The **TX_ADD**() macro takes a "snapshot" of the entire object referenced by
object handle *o* and saves it in the undo log. The object size is determined
from its *TYPE*. The application is then free to directly modify the object.
In case of a failure or abort, all the changes within the object will be
rolled back.

The **TX_XADD**() macro works exactly like **TX_ADD** when *flags* equals 0.
The *flags* argument is a bitmask of values as described in
**pmemobj_tx_xadd_range**, above.

The **TX_ADD_FIELD_DIRECT**() macro saves the current value of the given
*FIELD* of the object referenced by (direct) pointer *p* in the undo log.
The application is then free to directly modify the specified *FIELD*. In case
of a failure or abort, the saved value will be restored.

The **TX_XADD_FIELD_DIRECT**() macro works exactly like **TX_ADD_FIELD_DIRECT**
when *flags* equals 0. The *flags* argument is a bitmask of values as described
in **pmemobj_tx_xadd_range_direct**, above.

The **TX_ADD_DIRECT**() macro takes a "snapshot" of the entire object
referenced by (direct) pointer *p* and saves it in the undo log. The object
size is determined from its *TYPE*. The application is then free to directly
modify the object. In case of a failure or abort, all the changes within the
object will be rolled back.

The **TX_XADD_DIRECT**() macro works exactly like **TX_ADD_DIRECT** when
*flags* equals 0. The *flags* argument is a bitmask of values as described in
**pmemobj_tx_xadd_range_direct**, above.

The **TX_SET**() macro saves the current value of the given *FIELD* of the
object referenced by handle *o* in the undo log, and then sets its new *VALUE*.
In case of a failure or abort, the saved value will be restored.

The **TX_SET_DIRECT**() macro saves in the undo log the current value of given
*FIELD* of the object referenced by (direct) pointer *p*, and then set its new
*VALUE*. In case of a failure or abort, the saved value will be restored.

The **TX_MEMCPY**() macro saves in the undo log the current content of *dest*
buffer and then overwrites the first *num* bytes of its memory area with
the data copied from the buffer pointed by *src*. In case of a failure or abort,
the saved value will be restored.

The **TX_MEMSET**() macro saves the current content of the *dest* buffer in the
undo log and then fills the first *num* bytes of its memory area with the
constant byte *c*. In case of a failure or abort, the saved value will be
restored.


# RETURN VALUE #

On success, **pmemobj_tx_add_range**(), **pmemobj_tx_xadd_range**(),
**pmemobj_tx_add_range_direct**() and **pmemobj_tx_xadd_range_direct**()
return 0. Otherwise, the stage is changed to **TX_STAGE_ONABORT** and an error
number is returned.


# SEE ALSO #

**pmemobj_tx_alloc**(3), **pmemobj_tx_begin**(3),
**libpmemobj**(7) and **<http://pmem.io>**

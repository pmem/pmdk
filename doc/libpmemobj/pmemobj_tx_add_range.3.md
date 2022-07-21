---
draft: false
slider_enable: true
description: ""
disclaimer: "The contents of this web site and the associated <a href=\"https://github.com/pmem\">GitHub repositories</a> are BSD-licensed open source."
aliases: ["pmemobj_tx_add_range.3.html"]
title: "libpmemobj | PMDK"
header: "pmemobj API version 2.3"
---

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2017-2019, Intel Corporation)

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

+ **POBJ_XADD_NO_ABORT** - if the function does not end successfully,
do not abort the transaction.

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

+ **POBJ_XADD_NO_ABORT** - if the function does not end successfully,
do not abort the transaction.

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

On success, **pmemobj_tx_add_range**() and **pmemobj_tx_add_range_direct**()
return 0. Otherwise, the stage is changed to **TX_STAGE_ONABORT**,
**errno** is set appropriately and transaction is aborted.

On success, **pmemobj_tx_xadd_range**() and **pmemobj_tx_xadd_range_direct**()
returns 0. Otherwise, the error number is returned, **errno** is set and
when flags do not contain **POBJ_XADD_NO_ABORT**, the transaction is aborted.

# SEE ALSO #

**pmemobj_tx_alloc**(3), **pmemobj_tx_begin**(3),
**libpmemobj**(7) and **<https://pmem.io>**

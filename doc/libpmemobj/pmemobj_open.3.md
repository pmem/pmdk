---
draft: false
slider_enable: true
description: ""
disclaimer: "The contents of this web site and the associated <a href=\"https://github.com/pmem\">GitHub repositories</a> are BSD-licensed open source."
aliases: ["pmemobj_open.3.html"]
title: "libpmemobj | PMDK"
header: "pmemobj API version 2.3"
---

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2017-2023, Intel Corporation)

[comment]: <> (pmemobj_open.3 -- man page for most commonly used functions from libpmemobj library)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[CAVEATS](#caveats)<br />
[SEE ALSO](#see-also)<br />

# NAME #

_UW(pmemobj_open), _UW(pmemobj_create),
**pmemobj_close**(), _UW(pmemobj_check)
**pmemobj_set_user_data**(), **pmemobj_get_user_data**()
- create, open, close and validate persistent memory transactional object store

# SYNOPSIS #

```c
#include <libpmemobj.h>

_UWFUNCR1(PMEMobjpool, *pmemobj_open, *path, const char *layout)
_UWFUNCR1(PMEMobjpool, *pmemobj_create, *path, =q=const char *layout,
	size_t poolsize, mode_t mode=e=)
void pmemobj_close(PMEMobjpool *pop);
_UWFUNCR1(int, pmemobj_check, *path, const char *layout)

void pmemobj_set_user_data(PMEMobjpool *pop, void *data);
void *pmemobj_get_user_data(PMEMobjpool *pop);
```

_UNICODE()

# DESCRIPTION #

To use the pmem-resident transactional object store provided by
**libpmemobj**(7), a *memory pool* must first be created
with the _UW(pmemobj_create) function described below. Existing pools
may be opened with the _UW(pmemobj_open) function.

As of **libpmemobj** **1.11**, these functions are thread-safe; be careful
if you have to use earlier versions of the library.

Once created, the memory pool is represented by an opaque handle,
of type *PMEMobjpool\**, which is passed to most of the other **libpmemobj**(7)
functions. Internally, **libpmemobj**(7) will use either **pmem_persist**(3)
or **msync**(2) when it needs to flush changes, depending on whether the memory
pool appears to be persistent memory or a regular file (see the
**pmem_is_pmem**(3) function in **libpmem**(7) for more information). There is
no need for applications to flush changes directly when using the object
memory API provided by **libpmemobj**(7).

The _UW(pmemobj_create) function creates a transactional object store with the
given total *poolsize*. *path* specifies the name of the memory pool file to be
created. *layout* specifies the application's layout type in the form of a
string. The layout name is not interpreted by **libpmemobj**(7), but may be
used as a check when _UW(pmemobj_open) is called. The layout name, including
the terminating null byte ('\0'), cannot be longer than **PMEMOBJ_MAX_LAYOUT**
as defined in **\<libpmemobj.h\>**. A NULL *layout* is equivalent
to using an empty string as a layout name. *mode* specifies the permissions to
use when creating the file, as described by **creat**(2). The memory pool file
is fully allocated to the size *poolsize* using **posix_fallocate**(3). The
caller may choose to take responsibility for creating the memory pool file
by creating it before calling _UW(pmemobj_create), and then specifying
*poolsize* as zero. In this case _UW(pmemobj_create) will take the pool size
from the size of the existing file and will verify that the file appears to be
empty by searching for any non-zero data in the pool header at the beginning of
the file. The minimum net pool size allowed by the library for a local
transactional object store is defined in **\<libpmemobj.h\>** as
**PMEMOBJ_MIN_POOL**.

Depending on the configuration of the system, the available non-volatile
memory space may be divided into multiple memory devices.
In such case, the maximum size of the pmemobj memory pool
could be limited by the capacity of a single memory device.
**libpmemobj**(7) allows building persistent memory
resident object store spanning multiple memory devices by creation of
persistent memory pools consisting of multiple files, where each part of
such a *pool set* may be stored on a different memory device
or pmem-aware filesystem.

Creation of all the parts of the pool set can be done with _UW(pmemobj_create);
however, the recommended method for creating pool sets is with the
**pmempool**(1) utility.

When creating a pool set consisting of multiple files, the *path* argument
passed to _UW(pmemobj_create) must point to the special *set* file that defines
the pool layout and the location of all the parts of the pool set. The
*poolsize* argument must be 0. The meaning of the *layout* and *mode* arguments
does not change, except that the same *mode* is used for creation of all the
parts of the pool set.

The *set* file is a plain text file, the structure of which is described in
**poolset**(5).

The _UW(pmemobj_open) function opens an existing object store memory pool.
Similar to _UW(pmemobj_create), *path* must identify either an existing
obj memory pool file, or the *set* file used to create a pool set.
If *layout* is non-NULL, it is compared to the layout
name provided to _UW(pmemobj_create) when the pool was first created. This can
be used to verify that the layout of the pool matches what was expected.
The application must have permission to open the file and memory map it with
read/write permissions.

Be aware that if the pool contains bad blocks inside, opening can be aborted
by the SIGBUS signal, because currently the pool is not checked against
bad blocks during opening. It can be turned on by setting the CHECK_BAD_BLOCKS
compat feature. For details see description of this feature
in **pmempool-feature**(1).

The **pmemobj_close**() function closes the memory pool indicated by *pop* and
deletes the memory pool handle. The object store itself lives on in the file
that contains it and may be re-opened at a later time using
_UW(pmemobj_open) as described above.

The _UW(pmemobj_check) function performs a consistency check of the file
indicated by *path*. _UW(pmemobj_check) opens the given *path* read-only so
it never makes any changes to the file. This function is not supported on
Device DAX.

The **pmemobj_set_user_data**() function associates custom volatile state,
represented by pointer *data*, with the given pool *pop*. This state can later
be retrieved using **pmemobj_get_user_data**() function. This state does not
survive pool close. If **pmemobj_set_user_data**() was not called for a given
pool, **pmemobj_get_user_data**() will return NULL.

# RETURN VALUE #

The _UW(pmemobj_create) function returns a memory pool handle to be used with
most of the functions in **libpmemobj**(7). On error it returns NULL
and sets *errno* appropriately.

The _UW(pmemobj_open) function returns a memory pool handle to be used with
most of the functions in **libpmemobj**(7). If an error prevents the pool
from being opened, or if the given *layout* does not match the pool's layout,
_UW(pmemobj_open) returns NULL and sets *errno* appropriately.

The **pmemobj_close**() function returns no value.

The _UW(pmemobj_check) function returns 1 if the memory pool is found to be
consistent. Any inconsistencies found will cause _UW(pmemobj_check) to
return 0, in which case the use of the file with **libpmemobj**(7) will result
in undefined behavior. The debug version of **libpmemobj**(7) will provide
additional details on inconsistencies when **PMEMOBJ_LOG_LEVEL** is at least 1,
as described in the **DEBUGGING AND ERROR HANDLING** section in
**libpmemobj**(7). _UW(pmemobj_check) returns -1 and sets *errno* if it cannot
perform the consistency check due to other errors.

# CAVEATS #

Not all file systems support **posix_fallocate**(3). _UW(pmemobj_create) will
fail if the underlying file system does not support **posix_fallocate**(3).

# SEE ALSO #

**creat**(2), **msync**(2), **pmem_is_pmem**(3), **pmem_persist**(3),
**posix_fallocate**(3), **libpmem**(7), **libpmemobj**(7)
and **<https://pmem.io>**

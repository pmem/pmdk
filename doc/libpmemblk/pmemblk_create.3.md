---
layout: manual
Content-Style: 'text/css'
title: _MP(PMEMBLK_CREATE, 3)
collection: libpmemblk
header: PMDK
date: pmemblk API version 1.1
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2017-2018, Intel Corporation)

[comment]: <> (pmemblk_create.3 -- man page for libpmemblk create, open, close and validate functions)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />

# NAME #

_UW(pmemblk_create), _UW(pmemblk_open),
**pmemblk_close**(), _UW(pmemblk_check)
- create, open, close and validate block pool

# SYNOPSIS #

```c
#include <libpmemblk.h>

_UWFUNCR1(PMEMblkpool, *pmemblk_create, *path, =q=size_t bsize,
		size_t poolsize, mode_t mode=e=)
_UWFUNCR1(PMEMblkpool, *pmemblk_open, *path, size_t bsize)
void pmemblk_close(PMEMblkpool *pbp);
_UWFUNCR1(int, pmemblk_check, *path, size_t bsize)
```

_UNICODE()

# DESCRIPTION #

The _UW(pmemblk_create) function creates a block memory pool with the given
total *poolsize*, divided into as many elements of size *bsize* as will fit in
the pool. Since the transactional nature of a block memory pool requires some
space overhead in the memory pool, the resulting number of available blocks is
less than *poolsize*/*bsize*, and is made available to the caller via the
**pmemblk_nblock**(3) function. Given the specifics of the implementation, the
number of available blocks for the user cannot be less than 256. This
translates to at least 512 internal blocks. *path* specifies the name of the
memory pool file to be created. *mode* specifies the permissions to use when
creating the file, as described by **creat**(2). The memory pool file is fully
allocated to the size *poolsize* using **posix_fallocate**(3). The caller may
choose to take responsibility for creating the memory pool file by creating it
before calling _UW(pmemblk_create), and then specifying *poolsize* as zero. In
this case _UW(pmemblk_create) will take the pool size from the size of the
existing file, and will verify that the file appears to be empty by searching
for any non-zero data in the pool header at the beginning of the file. The net
pool size of a pool file is equal to the file size. The minimum net pool size
allowed by the library for a block pool is defined in **\<libpmemblk.h\>** as
**PMEMBLK_MIN_POOL**. *bsize* can be any non-zero value; however,
**libpmemblk** will silently round up
the given size to **PMEMBLK_MIN_BLK**, as defined in **\<libpmemblk.h\>**.

Depending on the configuration of the system, the available non-volatile
memory space may be divided into multiple memory devices. In such case, the
maximum size of the pmemblk memory pool could be limited by the capacity of a
single memory device. **libpmemblk**(7) allows building a persistent memory
resident array spanning multiple memory devices by creation of persistent
memory pools consisting of multiple files, where each part of such a *pool set*
may be stored on a different memory device or pmem-aware filesystem.

Creation of all the parts of the pool set can be done with _UW(pmemblk_create);
however, the recommended method for creating pool sets is by using the
**pmempool**(1) utility.

When creating a pool set consisting of multiple files, the *path* argument
passed to _UW(pmemblk_create) must point to the special *set* file that defines
the pool layout and the location of all the parts of the pool set. The
*poolsize* argument must be 0. The meaning of the *mode* argument
does not change, except that the same *mode* is used for creation of all the
parts of the pool set.

For more information on pool set format, see **poolset**(5).

The _UW(pmemblk_open) function opens an existing block memory pool.
As with _UW(pmemblk_create), *path* must identify either an existing
block memory pool file, or the *set* file used to create a pool set.
The application must have permission to open the file and memory map the
file or pool set with read/write permissions. If *bsize* is non-zero,
_UW(pmemblk_open) will verify that the given block size matches the block
size used when the pool was created. Otherwise, _UW(pmemblk_open) will open
the pool without verifying the block size. The *bsize* can be determined
using the **pmemblk_bsize**(3) function.

Be aware that if the pool contains bad blocks inside, opening can be aborted
by the SIGBUS signal, because currently the pool is not checked against
bad blocks during opening. It can be turned on by setting the CHECK_BAD_BLOCKS
compat feature. For details see description of this feature
in **pmempool-feature**(1).

The **pmemblk_close**() function closes the memory pool
indicated by *pbp* and deletes the memory pool handle.
The block memory pool itself lives on in the file that contains it and may be
re-opened at a later time using _UW(pmemblk_open) as described above.

The _UW(pmemblk_check) function performs a consistency check of the file
indicated by *path*, and returns 1 if the memory pool is found to be
consistent. If the pool is found not to be consistent, further use of the
file with **libpmemblk** will result in undefined behavior. The debug version
of **libpmemblk** will provide additional details on inconsistencies when
**PMEMBLK_LOG_LEVEL** is at least 1, as described in the **DEBUGGING AND ERROR
HANDLING** section in **libpmemblk**(7). _UW(pmemblk_check) opens the given
*path* read-only so it never makes any changes to the file. This function is
not supported on Device DAX.

# RETURN VALUE #

On success, _UW(pmemblk_create) returns a *PMEMblkpool\** handle to the block
memory pool. On error, it returns NULL and sets *errno* appropriately.

On success, _UW(pmemblk_open) returns a *PMEMblkpool\** handle that can be
used with most of the functions in **libpmemblk**(7). On error, it returns
NULL and sets *errno* appropriately. Possible errors include:

+ failure to open *path*

+ *path* specifies a *set* file and any of the pool set files cannot be opened

+ *path* specifies a *set* file and the actual size of any file does not
match the corresponding part size defined in the *set* file

+ *bsize* is non-zero and does not match the block size given when the pool
was created. *errno* is set to **EINVAL** in this case.

The **pmemblk_close**() function returns no value.

_UW(pmemblk_check) returns 1 if the memory pool is found to be consistent.
If the check is successfully performed but the pool is found to be inconsistent,
_UW(pmemblk_check) returns 0. This includes the case where *bsize* is non-zero
and does not match the block size given when the pool was created. If the
consistency check cannot be performed, _UW(pmemblk_check) returns -1 and sets
*errno* appropriately.

# CAVEATS #

Not all file systems support **posix_fallocate**(3). _UW(pmemblk_create) will
fail if the underlying file system does not support **posix_fallocate**(3).

_WINUX(=q= On Windows if _UW(pmemblk_create) is called on an existing file
with FILE_ATTRIBUTE_SPARSE_FILE and FILE_ATTRIBUTE_COMPRESSED set,
they will be removed, to physically allocate space for the pool.
This is a workaround for _chsize() performance issues. =e=)

# SEE ALSO #
**pmempool**(1), **creat**(2), **pmemblk_nblock**(3),
**posix_fallocate**(3), **poolset**(5),
**libpmemblk**(7) and **<https://pmem.io>**

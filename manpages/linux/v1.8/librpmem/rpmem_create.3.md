---
layout: manual
Content-Style: 'text/css'
title: RPMEM_CREATE
collection: librpmem
header: PMDK
date: rpmem API version 1.3
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

[comment]: <> (rpmem_create.3 -- man page for most commonly used librpmem functions)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[NOTES](#notes)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**rpmem_create**(), **rpmem_open**(),
**rpmem_set_attr**(), **rpmem_close**(), **rpmem_remove**()
- most commonly used functions for remote access to *persistent memory*

# SYNOPSIS #

```c
#include <librpmem.h>

RPMEMpool *rpmem_create(const char *target, const char *pool_set_name,
	void *pool_addr, size_t pool_size, unsigned *nlanes,
	const struct rpmem_pool_attr *create_attr);
RPMEMpool *rpmem_open(const char *target, const char *pool_set_name,
	void *pool_addr, size_t pool_size, unsigned *nlanes,
	struct rpmem_pool_attr *open_attr);
int rpmem_set_attr(RPMEMpool *rpp, const struct rpmem_pool_attr *attr);
int rpmem_close(RPMEMpool *rpp);
int rpmem_remove(const char *target, const char *pool_set_name, int flags);
```

# DESCRIPTION #

The **rpmem_create**() function creates a remote pool on a given *target* node,
using pool *set* file *pool_set_name* to map the remote pool. *pool_set_name*
is a relative path in the root config directory on the *target* node.
For pool set file format and options see **poolset**(5).
*pool_addr* is a pointer to the associated local memory pool with size
*pool_size*. Both *pool_addr* and *pool_size* must be aligned to the system's
page size (see **sysconf**(3)). The size of the remote pool must be at least
*pool_size*. See **REMOTE POOL SIZE**, below, for details.
*nlanes* points to the maximum number of lanes which the caller is requesting.
Upon successful creation of the remote pool, \**nlanes* is set to the
maximum number of lanes supported by both the local and remote nodes.
See **LANES**, below, for details.
The *create_attr* structure contains the attributes used for creating the
remote pool. If the *create_attr* structure is not NULL, a pool with internal
metadata is created. The metadata is stored in the first 4096
bytes of the pool and can be read when opening the remote pool with
**rpmem_open**(). To prevent user from overwriting the pool metadata, this
region is not accessible to the user via **rpmem_persist**().
If *create_attr* is NULL or zeroed, remote pool set file must contain
the *NOHDRS* option. In that case the remote pool is created without internal
metadata in it and the entire pool space is available to the user.
See **rpmem_persist**(3) for details.

The **rpmem_open**() function opens the existing remote pool with *set* file
*pool_set_name* on remote node *target*. *pool_set_name* is a relative path
in the root config directory on the *target* node. *pool_addr* is a pointer to
the associated local memory pool of size *pool_size*.
Both *pool_addr* and *pool_size* must be aligned to the system's page
size (see **sysconf**(3)). The size of the remote pool must be at least
*pool_size*. See **REMOTE POOL SIZE**, below, for details.
*nlanes* points to the maximum number of lanes which the caller is requesting.
Upon successful opening of the remote pool, \**nlanes* is set to the
maximum number of lanes supported by both the local and remote nodes.
See **LANES**, below, for details.

The **rpmem_set_attr**() function overwrites the pool's attributes.
The *attr* structure contains the attributes used for overwriting the remote
pool attributes that were passed to **rpmem_create**() at pool creation.
If *attr* is NULL, a zeroed structure with attributes will be used.
New attributes are stored in the pool's metadata.

The **rpmem_close**() function closes the remote pool *rpp*. All resources
are released on both the local and remote nodes. The remote pool itself
persists on the remote node and may be re-opened at a later time using
**rpmem_open**().

The **rpmem_remove**() function removes the remote pool with *set* file name
*pool_set_name* from node *target*. The *pool_set_name* is a relative path in
the root config directory on the *target* node. By default only the pool part
files are removed; the pool *set* file is left untouched. If the pool is not
consistent, the **rpmem_remove**() function fails.
The *flags* argument determines the behavior of **rpmem_remove**(). *flags* may
be either 0 or the bitwise OR of one or more of the following flags:

+ **RPMEM_REMOVE_FORCE** - Ignore errors when opening an inconsistent pool.
The pool *set* file must still be in appropriate format for the pool to be
removed.

+ **RPMEM_REMOVE_POOL_SET** - Remove the pool *set* file after removing the
pool described by this pool set.

# RETURN VALUE #

On success, **rpmem_create**() returns an opaque handle to the remote pool
for use in subsequent **librpmem** calls. If any error prevents
the remote pool from being created, **rpmem_create**() returns
NULL and sets *errno* appropriately.

On success, **rpmem_open**() returns an opaque handle to the remote
pool for subsequent **librpmem** calls. If the *open_attr* argument
is not NULL, the remote pool attributes are returned in the provided structure.
If the remote pool was created without internal metadata, zeroes are returned
in the *open_attr* structure on successful call to **rpmem_open**().
If any error prevents the remote pool from being opened, **rpmem_open**()
returns NULL and sets *errno* appropriately.

On success, **rpmem_set_attr**() returns 0. On error, it returns -1 and sets
*errno* appropriately.

On success, **rpmem_close**() returns 0. On error, it returns a non-zero value
and sets *errno* appropriately.

On success, **rpmem_remove**() returns 0. On error, it returns a non-zero value
and sets *errno* appropriately.

# NOTES #

## REMOTE POOL SIZE ##
The size of a remote pool depends on the configuration in the pool set file
on the remote node (see **poolset**(5)). If no pool set options is used in
the remote pool set file, the remote pool size is the sum of the sizes of all
part files, decreased by 4096 bytes per part file. 4096 bytes of each part file
are utilized for storing internal metadata.
If the *SINGLEHDR* option is used in the remote pool set file, the remote pool
size is the sum of sizes of all part files, decreased once by 4096 bytes.
In this case only the first part contains internal metadata.
If a remote pool set file contains the *NOHDRS* option, the remote pool size
is the sum of sizes of all its part files. In this case none of the parts
contains internal metadata. For other consequences of using the *SINGLEHDR* and
*NOHDRS* options see **rpmem_persist**(3).
**RPMEM_MIN_PART** and **RPMEM_MIN_POOL** in **\<librpmem.h\>** define
the minimum size allowed by **librpmem** for a part file and a remote pool,
respectively.

## LANES ##
The term *lane* means an isolated path of execution. The underlying hardware
utilized by both local and remote nodes may have limited resources that
restrict the maximum number of parallel **rpmem_persist**(3) operations.
The maximum number of supported lanes is returned by the **rpmem_open**() and
**rpmem_create**() function calls. The caller passes the maximum number of
lanes requested in \**nlanes*. If the pool is successfully created or opened,
\**nlanes* is updated to reflect the minimum of the number of lanes requested
by the caller and the maximum number of lanes supported by underlying hardware.
The application is obligated to use at most the returned number of
lanes in parallel.

**rpmem_persist**(3) does not provide any locking mechanism; thus any
serialization of calls must be performed by the application if required.

Each lane requires a separate connection, represented by a file descriptor.
If the system runs out of free file descriptors during **rpmem_create**() or
**rpmem_open**(), these functions will fail. See **nofile** in
**limits.conf**(5) for more details.

# SEE ALSO #

**rpmem_persist**(3), **sysconf**(3), **limits.conf**(5),
**libpmemobj**(7), **librpmem**(7) and **<https://pmem.io>**

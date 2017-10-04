---
layout: manual
Content-Style: 'text/css'
title: RPMEM_CREATE!3
collection: librpmem
header: NVM Library
date: rpmem API version 1.1
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

[comment]: <> (rpmem_create.3 -- page for most commonly used librpmem functions)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[NOTES](#notes)<br />
[SEE ALSO](#see-also)<br />


# NAME #

**rpmem_create**(), **rpmem_open**(),
**rpmem_set_attr**(), **rpmem_close**()
-- most commonly used functions to remote
access to *persistent memory*


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
```


# DESCRIPTION #

The **rpmem_create**() function creates a remote pool on a given *target* node.
The *pool_set_name* is a relative path in the root config directory on
the *target* node that uniquely identifies the pool set file on remote node
to be used when mapping the remote pool. The *pool_addr* is a pointer to the
associated local memory pool of a given size specified by the *pool_size*
argument. Both *pool_addr* and *pool_size* must be aligned to system's page
size (see **sysconf**(3)). The size of the remote pool must be at least
*pool_size*. See **REMOTE POOL SIZE** section for details.
The *nlanes* points to the maximum number of lanes which the caller requests to
use. Upon successful creation of the remote pool, the *nlanes* contains the
maximum number of lanes supported by both local and remote nodes' hardware.
See **LANES** subsection in **NOTES** section for details.
The *create_attr* structure contains the attributes used for creating the
remote pool. If *create_attr* is NULL, a zeroed structure with attributes will
be used to create the pool. The attributes are stored in pool's meta-data and
can be read when opening the remote pool using **rpmem_open**() function call

The **rpmem_open**() function opens an existing remote pool on a given *target*
node. The *pool_set_name* is a relative path in the root config directory on
the *target* node that uniquely identifies the pool set file on remote node
to be used when mapping the remote pool. The *pool_addr* is a pointer to the
associated local memory pool of a given size specified by the *pool_size*
argument. Both *pool_addr* and *pool_size* must be aligned to system's page
size (see **sysconf**(3)). The size of the remote pool must be at least
*pool_size*. See **REMOTE POOL SIZE** subsection in **NOTES** section for details.
The *nlanes* points to the maximum number of lanes which the caller requests to
use. Upon successful opening of the remote pool, the *nlanes* contains the
maximum number of lanes supported by both local and remote nodes' hardware.
See **LANES** subsection in **NOTES** section for details.

The **rpmem_set_attr**() function overwrites pool's attributes.
The *attr* structure contains the attributes used for overwriting the remote
pool attributes that were passed to **rpmem_create**() at pool's creation.
If *attr* is NULL, a zeroed structure with attributes will be used.
New attributes are stored in pool's meta-data.

The **rpmem_close**() function closes a remote pool indicated by *rpp*.
All resources are released on both local and remote side. The pool itself lives
on the remote node and may be re-opened at a later time using **rpmem_open**()
function as described above.

# RETURN VALUE #

The **rpmem_create**() upon success returns an opaque handle to the remote pool
which shall be used in subsequent API calls. If any error prevents the
**librpmem** from creating the remote pool, the **rpmem_create**() returns
NULL and sets *errno* appropriately.

The **rpmem_open**() function upon success returns an opaque handle to the remote
pool which shall be used in subsequent API calls. If any error prevents the
**librpmem** from opening the remote pool, the **rpmem_open**() returns NULL
and sets *errno* appropriately. If the *open_attr* argument is not NULL the remote
pool attributes are returned by the provided structure.

The **rpmem_set_attr**() function on success returns zero, on error it returns -1.

The **rpmem_close**() function on success it returns zero, if any error occurred
when closing remote pool, non-zero value is returned and *errno* value is set.


# NOTES #

## REMOTE POOL SIZE ##
A remote pool size depends on the configuration of a pool set file on the remote
node. The remote pool size is a sum of sizes of all part files decreased by 4096
bytes per each part file. The 4096 bytes of each part file is utilized for
storing internal metadata of the pool part files. The minimum size of a part
file for a remote pool is defined as **RPMEM_MIN_PART** in **\<librpmem.h\>**.
The minimum size of a remote pool allowed by the library is defined as
**RPMEM_MIN_POOL** therein.

## LANES ##
The term *lane* means an isolated path of execution. Due to a limited resources
provided by underlying hardware utilized by both local and remote nodes the
maximum number of parallel **rpmem_persist**(3) operations is limited by the
maximum number of lanes returned from either the **rpmem_open**() or
**rpmem_create**() function calls. The caller passes the maximum number of lanes
one would like to utilize. If the pool has been successfully created or opened,
the lanes value is updated to the minimum of: the number of lanes requested by
the caller and the maximum number of lanes supported by underlying hardware.
The application is obligated to use at most the returned number of
lanes in parallel. The **rpmem_persist**(3) does not provide any locking mechanism
thus the serialization of the calls shall be performed by the application if
required.

Each lane requires separate connection which is represented by the file descriptor.
If system will run out of free file descriptors during **rpmem_create**() or
**rpmem_open**() these functions will fail. See **nofile** in **limits.conf**(5)
for more details.


# SEE ALSO #

**rpmem_persist**(3), **sysconf**(3), **limits.conf**(5),
**libpmemobj**(7) and **<http://pmem.io>**

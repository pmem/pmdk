---
layout: manual
Content-Style: 'text/css'
title: _MP(LIBRPMEM, 7)
collection: librpmem
header: PMDK
date: rpmem API version 1.3
...

[comment]: <> (Copyright 2016-2019, Intel Corporation)

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

[comment]: <> (librpmem.7 -- man page for librpmem)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[TARGET NODE ADDRESS FORMAT](#target-node-address-format)<br />
[REMOTE POOL ATTRIBUTES](#remote-pool-attributes)<br />
[SSH](#ssh)<br />
[FORK](#fork)<br />
[CAVEATS](#caveats)<br />
[LIBRARY API VERSIONING](#library-api-versioning-1)<br />
[ENVIRONMENT](#environment)<br />
[DEBUGGING AND ERROR HANDLING](#debugging-and-error-handling)<br />
[EXAMPLE](#example)<br />
[ACKNOWLEDGEMENTS](#acknowledgements)<br />
[SEE ALSO](#see-also)

# NAME #

**librpmem** - remote persistent memory support library (EXPERIMENTAL)

# SYNOPSIS #

```c
#include <librpmem.h>
cc ... -lrpmem
```

##### Library API versioning: #####

```c
const char *rpmem_check_version(
	unsigned major_required,
	unsigned minor_required);
```

##### Error handling: #####

```c
const char *rpmem_errormsg(void);
```

##### Other library functions: #####

A description of other **librpmem** functions can be found on the following
manual pages:

+ **rpmem_create**(3), **rpmem_persist**(3)

# DESCRIPTION #

**librpmem** provides low-level support for remote access to
*persistent memory* (pmem) utilizing RDMA-capable RNICs. The library can be
used to remotely replicate a memory region over the RDMA protocol. It utilizes
an appropriate persistency mechanism based on the remote node's platform
capabilities. **librpmem** utilizes the **ssh**(1) client to authenticate
a user on the remote node, and for encryption of the connection's out-of-band
configuration data. See **SSH**, below, for details.

The maximum replicated memory region size can not be bigger than the maximum
locked-in-memory address space limit. See **memlock** in **limits.conf**(5)
for more details.

This library is for applications that use remote persistent memory directly,
without the help of any library-supplied transactions or memory
allocation. Higher-level libraries that build on **libpmem**(7) are
available and are recommended for most applications, see:

+ **libpmemobj**(7), a general use persistent memory API, providing memory
allocation and transactional operations on variable-sized objects.

# TARGET NODE ADDRESS FORMAT #

```
[<user>@]<hostname>[:<port>]
```

The target node address is described by the *hostname* which the client
connects to, with an optional *user* name. The user must be authorized
to authenticate to the remote machine without querying for password/passphrase.
The optional *port* number is used to establish the SSH connection. The default
port number is 22.

# REMOTE POOL ATTRIBUTES #

The *rpmem_pool_attr* structure describes a remote pool and is stored in remote
pool's metadata. This structure must be passed to the **rpmem_create**(3)
function by caller when creating a pool on remote node. When opening the pool
using **rpmem_open**(3) function the appropriate fields are read from pool's
metadata and returned back to the caller.

```c
#define RPMEM_POOL_HDR_SIG_LEN    8
#define RPMEM_POOL_HDR_UUID_LEN   16
#define RPMEM_POOL_USER_FLAGS_LEN 16

struct rpmem_pool_attr {
	char signature[RPMEM_POOL_HDR_SIG_LEN];
	uint32_t major;
	uint32_t compat_features;
	uint32_t incompat_features;
	uint32_t ro_compat_features;
	unsigned char poolset_uuid[RPMEM_POOL_HDR_UUID_LEN];
	unsigned char uuid[RPMEM_POOL_HDR_UUID_LEN];
	unsigned char next_uuid[RPMEM_POOL_HDR_UUID_LEN];
	unsigned char prev_uuid[RPMEM_POOL_HDR_UUID_LEN];
	unsigned char user_flags[RPMEM_POOL_USER_FLAGS_LEN];
};
```

The *signature* field is an 8-byte field which describes the pool's on-media
format.

The *major* field is a major version number of the pool's on-media format.

The *compat_features* field is a mask describing compatibility of pool's
on-media format optional features.

The *incompat_features* field is a mask describing compatibility of pool's
on-media format required features.

The *ro_compat_features* field is a mask describing compatibility of pool's
on-media format features. If these features are not available,
the pool shall be opened in read-only mode.

The *poolset_uuid* field is an UUID of the pool which the remote pool is
associated with.

The *uuid* field is an UUID of a first part of the remote pool. This field can
be used to connect the remote pool with other pools in a list.

The *next_uuid* and *prev_uuid* fields are UUIDs of next and previous replicas
respectively. These fields can be used to connect the remote pool with other
pools in a list.

The *user_flags* field is a 16-byte user-defined flags.

# SSH #

**librpmem** utilizes the **ssh**(1) client to login and execute the
**rpmemd**(1) process on the remote node. By default, **ssh**(1)
is executed with the **-4** option, which forces using **IPv4** addressing.

For debugging purposes, both the ssh client and the commands executed
on the remote node may be overridden by setting the **RPMEM_SSH** and
**RPMEM_CMD** environment variables, respectively. See **ENVIRONMENT**
for details.

# FORK #
The **ssh**(1) client is executed
by **rpmem_open**(3) and **rpmem_create**(3) after forking a child process
using **fork**(2).  The application must take this into account when
using **wait**(2) and **waitpid**(2), which may return the *PID* of
the **ssh**(1) process executed by **librpmem**.

If **fork**(2) support is not enabled in **libibverbs**,
**rpmem_open**(3) and **rpmem_create**(3) will fail.
By default, **fabric**(7) initializes **libibverbs** with **fork**(2) support
by calling the **ibv_fork_init**(3) function. See **fi_verbs**(7) for more
details.

# CAVEATS #

**librpmem** relies on the library destructor being called from the main thread.
For this reason, all functions that might trigger destruction (e.g.
**dlclose**(3)) should be called in the main thread. Otherwise some of the
resources associated with that thread might not be cleaned up properly.

**librpmem** registers a pool as a single memory region. A Chelsio T4 and T5
hardware can not handle a memory region greater than or equal to 8GB due to
a hardware bug. So *pool_size* value for **rpmem_create**(3) and **rpmem_open**(3)
using this hardware can not be greater than or equal to 8GB.

# LIBRARY API VERSIONING #

This section describes how the library API is versioned,
allowing applications to work with an evolving API.

The **rpmem_check_version**() function is used to see if the installed
**librpmem** supports the version of the library API required by an
application. The easiest way to do this is for the application to supply
the compile-time version information, supplied by defines in
**\<librpmem.h\>**, like this:

```c
reason = rpmem_check_version(RPMEM_MAJOR_VERSION,
                             RPMEM_MINOR_VERSION);
if (reason != NULL) {
	/* version check failed, reason string tells you why */
}
```

Any mismatch in the major version number is considered a failure, but a
library with a newer minor version number will pass this check since
increasing minor versions imply backwards compatibility.

An application can also check specifically for the existence of an
interface by checking for the version where that interface was
introduced. These versions are documented in this man page as follows:
unless otherwise specified, all interfaces described here are available
in version 1.0 of the library. Interfaces added after version 1.0 will
contain the text *introduced in version x.y* in the section of this
manual describing the feature.

When the version check performed by **rpmem_check_version**() is
successful, the return value is NULL. Otherwise the return value is a
static string describing the reason for failing the version check. The
string returned by **rpmem_check_version**() must not be modified or
freed.

# ENVIRONMENT #

**librpmem** can change its default behavior based on the following
environment variables. These are largely intended for testing and are
not normally required.

+ **RPMEM_SSH**=*ssh_client*

Setting this environment variable overrides the default **ssh**(1) client
command name.

+ **RPMEM_CMD**=*cmd*

Setting this environment variable overrides the default command executed on
the remote node using either **ssh**(1) or the alternative remote shell command
specified by **RPMEM_SSH**.

**RPMEM_CMD** can contain multiple commands separated by a vertical bar (`|`).
Each consecutive command is executed on the remote node in order read from a
pool set file. This environment variable is read when the library is
initialized, so **RPMEM_CMD** must be set prior to application launch (or
prior to **dlopen**(3) if **librpmem** is being dynamically loaded).

+ **RPMEM_ENABLE_SOCKETS**=0\|1

Setting this variable to 1 enables using **fi_sockets**(7) provider for
in-band RDMA connection. The *sockets* provider does not support IPv6.
It is required to disable IPv6 system wide if **RPMEM_ENABLE_SOCKETS** == 1 and
*target* == localhost (or any other loopback interface address) and
**SSH_CONNECTION** variable (see **ssh**(1) for more details) contains IPv6
address after ssh to loopback interface. By default the *sockets* provider is
disabled.

* **RPMEM_ENABLE_VERBS**=0\|1

Setting this variable to 0 disables using **fi_verbs**(7) provider for
in-band RDMA connection. The *verbs* provider is enabled by default.

* **RPMEM_MAX_NLANES**=*num*

Limit the maximum number of lanes to *num*. See **LANES**, in **rpmem_create**(3), for details.

* **RPMEM_WORK_QUEUE_SIZE**=*size*

Suggest the work queue size. The effective work queue size can be greater than
suggested if **librpmem** requires it or it can be smaller if underlying hardware
does not support the suggested size. The work queue size affects the performance
of communication to the remote node.
**rpmem_flush**(3) operations can be added to the work queue up to the size of
this queue. When work queue is full any subsequent call has to wait till the work
queue will be drained. **rpmem_drain**(3) and **rpmem_persist**(3) among other
things also drain the work queue.

# DEBUGGING AND ERROR HANDLING #

If an error is detected during the call to a **librpmem** function, the
application may retrieve an error message describing the reason for the failure
from **rpmem_errormsg**(). This function returns a pointer to a static buffer
containing the last error message logged for the current thread. If *errno*
was set, the error message may include a description of the corresponding
error code as returned by **strerror**(3). The error message buffer is
thread-local; errors encountered in one thread do not affect its value in
other threads. The buffer is never cleared by any library function; its
content is significant only when the return value of the immediately preceding
call to a **librpmem** function indicated an error, or if *errno* was set.
The application must not modify or free the error message string, but it may
be modified by subsequent calls to other library functions.

Two versions of **librpmem** are typically available on a development
system. The normal version, accessed when a program is linked using the
**-lrpmem** option, is optimized for performance. That version skips checks
that impact performance and never logs any trace information or performs any
run-time assertions.

A second version of **librpmem**, accessed when a program uses the libraries
under _DEBUGLIBPATH(), contains run-time assertions and trace points. The
typical way to access the debug version is to set the environment variable
**LD_LIBRARY_PATH** to _LDLIBPATH(). Debugging output is
controlled using the following environment variables. These variables have
no effect on the non-debug version of the library.

+ **RPMEM_LOG_LEVEL**

The value of **RPMEM_LOG_LEVEL** enables trace points in the debug version
of the library, as follows:

+ **0** - This is the default level when **RPMEM_LOG_LEVEL** is not set.
No log messages are emitted at this level.

+ **1** - Additional details on any errors detected are logged
(in addition to returning the *errno*-based errors as usual).
The same information may be retrieved using **rpmem_errormsg**().

+ **2** - A trace of basic operations is logged.

+ **3** - Enables a very verbose amount of function call
tracing in the library.

+ **4** - Enables voluminous and fairly obscure tracing information
that is likely only useful to the **librpmem** developers.

Unless **RPMEM_LOG_FILE** is set, debugging output is written to *stderr*.

+ **RPMEM_LOG_FILE**

Specifies the name of a file where all logging information should be written.
If the last character in the name is "-", the *PID* of the current process will
be appended to the file name when the log file is created. If
**RPMEM_LOG_FILE** is not set, logging output is written to *stderr*.

# EXAMPLE #

The following example uses **librpmem** to create a remote pool on given
target node identified by given pool set name. The associated local memory
pool is zeroed and the data is made persistent on remote node. Upon success
the remote pool is closed.

```c
#include <assert.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <librpmem.h>

#define POOL_SIGNATURE	"MANPAGE"
#define POOL_SIZE	(32 * 1024 * 1024)
#define NLANES		4

#define DATA_OFF	4096
#define DATA_SIZE	(POOL_SIZE - DATA_OFF)

static void
parse_args(int argc, char *argv[], const char **target, const char **poolset)
{
	if (argc < 3) {
		fprintf(stderr, "usage:\t%s <target> <poolset>\n", argv[0]);
		exit(1);
	}

	*target = argv[1];
	*poolset = argv[2];
}

static void *
alloc_memory()
{
	long pagesize = sysconf(_SC_PAGESIZE);
	if (pagesize < 0) {
		perror("sysconf");
		exit(1);
	}

	/* allocate a page size aligned local memory pool */
	void *mem;
	int ret = posix_memalign(&mem, pagesize, POOL_SIZE);
	if (ret) {
		fprintf(stderr, "posix_memalign: %s\n", strerror(ret));
		exit(1);
	}

	assert(mem != NULL);

	return mem;
}

int
main(int argc, char *argv[])
{
	const char *target, *poolset;
	parse_args(argc, argv, &target, &poolset);

	unsigned nlanes = NLANES;
	void *pool = alloc_memory();
	int ret;

	/* fill pool_attributes */
	struct rpmem_pool_attr pool_attr;
	memset(&pool_attr, 0, sizeof(pool_attr));
	strncpy(pool_attr.signature, POOL_SIGNATURE, RPMEM_POOL_HDR_SIG_LEN);

	/* create a remote pool */
	RPMEMpool *rpp = rpmem_create(target, poolset, pool, POOL_SIZE,
			&nlanes, &pool_attr);
	if (!rpp) {
		fprintf(stderr, "rpmem_create: %s\n", rpmem_errormsg());
		return 1;
	}

	/* store data on local pool */
	memset(pool, 0, POOL_SIZE);

	/* make local data persistent on remote node */
	ret = rpmem_persist(rpp, DATA_OFF, DATA_SIZE, 0, 0);
	if (ret) {
		fprintf(stderr, "rpmem_persist: %s\n", rpmem_errormsg());
		return 1;
	}

	/* close the remote pool */
	ret = rpmem_close(rpp);
	if (ret) {
		fprintf(stderr, "rpmem_close: %s\n", rpmem_errormsg());
		return 1;
	}

	free(pool);

	return 0;
}
```

# NOTE #

The **librpmem** API is experimental and may be subject to change in the future.
However, using the remote replication in **libpmemobj**(7) is safe and backward
compatibility will be preserved.

# ACKNOWLEDGEMENTS #

**librpmem** builds on the persistent memory programming model
recommended by the SNIA NVM Programming Technical Work Group:
<http://snia.org/nvmp>

# SEE ALSO #

**rpmemd**(1), **ssh**(1), **fork**(2), **dlclose**(3), **dlopen**(3),
**ibv_fork_init**(3), **rpmem_create**(3), **rpmem_drain**(3), **rpmem_flush**(3),
**rpmem_open**(3), **rpmem_persist**(3), **strerror**(3), **limits.conf**(5),
**fabric**(7), **fi_sockets**(7), **fi_verbs**(7), **libpmem**(7), **libpmemblk**(7),
**libpmemlog**(7), **libpmemobj**(7)
and **<http://pmem.io>**

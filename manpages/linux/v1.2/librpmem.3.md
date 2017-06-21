---
layout: manual
Content-Style: 'text/css'
title: librpmem
header: NVM Library
date: rpmem API version 1.0.2
...

[comment]: <> (Copyright 2016, Intel Corporation)

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

[comment]: <> (librpmem.3 -- man page for librpmem)


[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[MOST COMMONLY USED FUNCTIONS](#most-commonly-used-functions-1)<br />
[LANES](#lanes)<br />
[TARGET NODE ADDRESS FORMAT](#target-node-address-format)<br />
[REMOTE POOL ATTRIBUTES](#remote-pool-attributes)<br />
[SSH](#ssh)<br />
[LIBRARY API VERSIONING](#library-api-versioning-1)<br />
[DEBUGGING AND ERROR HANDLING](#debugging-and-error-handling)<br />
[ENVIRONMENT](#environment)<br />
[EXAMPLE](#example)<br />
[ACKNOWLEDGEMENTS](#acknowledgements)<br />
[SEE ALSO](#see-also)


# NAME #

**librpmem** -- remote persistent memory support library


# SYNOPSIS #

```c
#include <librpmem.h>
cc ... -lrpmem
```

##### Most commonly used functions: #####

```c
RPMEMpool *rpmem_create(const char *target, const char *pool_set_name,
	void *pool_addr, size_t pool_size, unsigned *nlanes,
	const struct rpmem_pool_attr *create_attr);
RPMEMpool *rpmem_open(const char *target, const char *pool_set_name,
	void *pool_addr, size_t pool_size, unsigned *nlanes,
	struct rpmem_pool_attr *open_attr);
int rpmem_close(RPMEMpool *rpp);

int rpmem_persist(RPMEMpool *rpp, size_t offset, size_t length, unsigned lane);
int rpmem_read(RPMEMpool *rpp, void *buff, size_t offset, size_t length);
int rpmem_remove(const char *target, const char *pool_set_name, int flags);
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


# DESCRIPTION #

**librpmem** provides low-level support for remote access to
*persistent memory* (pmem) utilizing RDMA-capable RNICs. The library can be
used to replicate remotely a memory region over RDMA protocol.
It utilizes appropriate persistency mechanism based on remote node's platform
capabilities. The **librpmem** utilizes the **ssh** client to authenticate
a user on remote node and for encryption of connection's out-of-band
configuration data. See **SSH** section for details.

This library is for applications that use remote persistent memory directly,
without the help of any library-supplied transactions or memory
allocation. Higher-level libraries that build on **libpmem** are
available and are recommended for most applications, see:

+ **libpmemobj**(3), a general use persistent memory API, providing memory
allocation and transactional operations on variable-sized objects.


# MOST COMMONLY USED FUNCTIONS #

```c
RPMEMpool *rpmem_create(const char *target, const char *pool_set_name,
	void *pool_addr, size_t pool_size, unsigned *nlanes,
	const struct rpmem_pool_attr *create_attr);
```

The **rpmem_create**() function creates a remote pool on a given *target* node.
The *pool_set_name* is a relative path in the root config directory on
the *target* node that uniquely identifies the pool set file on remote node
to be used when mapping the remote pool. The *pool_addr* is a pointer to the
associated local memory pool of a given size specified by the *pool_size*
argument. Both *pool_addr* and *pool_size* must be aligned to system's page
size (see **sysconf**(3)). The size of the remote pool must be at least
*pool_size*. See **REMOTE POOL SIZE** section for details.
The *nlanes* points to the maximum number of lanes which the caller requests to
use. Upon successfully opening of the remote pool, the *nlanes* contains the
maximum number of lanes supported by both local and remote nodes' hardware.
See **LANES** section for details.
The *create_attr* structure contains the attributes used for creating the
remote pool. If *create_attr* is NULL, a zeroed structure with attributes will
be used to create the pool. The attributes are stored in pool's meta-data and
can be read when opening the remote pool using **rpmem_open**() function call.
Upon success the **rpmem_create**() returns an opaque handle to the remote pool
which shall be used in subsequent API calls. If any error prevents the
**librpmem** from creating the remote pool, the **rpmem_create**() returns
NULL and sets *errno* appropriately.

```c
RPMEMpool *rpmem_open(const char *target, const char *pool_set_name,
	void *pool_addr, size_t pool_size, unsigned *nlanes,
	struct rpmem_pool_attr *open_attr);
```

The **rpmem_open**() function opens an existing remote pool on a given *target*
node. The *pool_set_name* is a relative path in the root config directory on
the *target* node that uniquely identifies the pool set file on remote node
to be used when mapping the remote pool. The *pool_addr* is a pointer to the
associated local memory pool of a given size specified by the *pool_size*
argument. Both *pool_addr* and *pool_size* must be aligned to system's page
size (see **sysconf**(3)). The size of the remote pool must be at least
*pool_size*. See **REMOTE POOL SIZE** section for details.
The *nlanes* points to the maximum number of lanes which the caller requests to
use. Upon successfully opening of the remote pool, the *nlanes* contains the
maximum number of lanes supported by both local and remote nodes' hardware.
See **LANES** section for details.
If the *open_attr* argument is not NULL the remote pool attributes
are returned by the provided structure. Upon success the **rpmem_open**()
returns an opaque handle to the remote pool which shall be used in
subsequent API calls. If any error prevents the **librpmem** from opening the
remote pool, the **rpmem_open**() returns NULL and sets *errno*
appropriately.

```c
int rpmem_close(RPMEMpool *rpp);
```

The **rpmem_close**() function closes a remote pool indicated by *rpp*.
All resources are released on both local and remote side. The pool itself lives
on the remote node and may be re-opened at a later time using **rpmem_open**()
function as described above. If any error occurred when closing remote pool,
non-zero value is returned and *errno* value is set.

```c
int rpmem_persist(RPMEMpool *rpp, size_t offset, size_t length, unsigned lane);
```

The **rpmem_persist**() function copies data of given *length* at given
*offset* from the associated local memory pool and makes sure the data is
persistent on the remote node before the function returns. The remote node
is identified by the *rpp* handle which must be returned from either
**rpmem_open**() or **rpmem_create**() functions. The *offset* is relative
to the *pool_addr* specified in the **rpmem_open**() or **rpmem_create**()
function calls. The *offset* and *length* combined must not exceed the
*pool_size* passed to the **rpmem_open**() or **rpmem_create**() functions.
The **rpmem_persist**() operation is performed using given *lane* number.
The lane must be less than the value returned by **rpmem_open**() or
**rpmem_create**() through the *nlanes* argument (so it can take a value
from 0 to *nlanes* - 1).
If the entire memory area was made persistent on remote node
the **rpmem_persist**() returns 0, otherwise it returns non-zero value
and sets *errno* appropriately.

```c
int rpmem_read(RPMEMpool *rpp, void *buff, size_t offset, size_t length);
```

The **rpmem_read**() function reads *length* bytes of data from remote pool
at *offset* and copies it to the buffer *buff*. The function returns 0
if the data was read entirely, otherwise non-zero value is returned and
*errno* set appropriately.
The *rpp* must point to a remote pool opened or created previously by
**rpmem_open**() or **rpmem_create**() functions respectively.

```c
int rpmem_remove(const char *target, const char *pool_set_name, int flags);
```

The **rpmem_remove**() function removes a remote pool on a given *target* node.
The *pool_set_name* is a relative path in the root config directory on the
*target* node that uniquely identifies the pool set file on remote node.
By default only the pool part files are removed and pool set file is left
untouched. If the pool is not consistent the **rpmem_remove**() function fails,
unless otherwise specified.
The *flags* argument determines the behavior of **rpmem_remove**() function.
It is either 0 or the bitwise OR of one or more of the following flags:

+ **RPMEM_REMOVE_FORCE**
Ignore errors when opening inconsistent pool. The pool set file must be in
appropriate format though.

+ **RPMEM_REMOVE_POOL_SET**
Remove pool set file after removing the pool described by this pool set.


# LANES #

The term *lane* means an isolated path of execution. Due to a limited resources
provided by underlying hardware utilized by both local and remote nodes the
maximum number of parallel **rpmem_persist**() operations is limited by the
maximum number of lanes returned from either the **rpmem_open**() or
**rpmem_create**() function calls. The caller passes the maximum number of lanes
one would like to utilize. If the pool has been successfully created or opened,
the lanes value is updated to the minimum of: the number of lanes requested by
the caller and the maximum number of lanes supported by underlying hardware.
The application is obligated to use at most the returned number of
lanes in parallel. The **rpmem_persist**() does not provide any locking mechanism
thus the serialization of the calls shall be performed by the application if
required.


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
pool's metadata. This structure must be passed to the **rpmem_create**()
function by caller when creating a pool on remote node. When opening the pool
using **rpmem_open**() function the appropriate fields are read from pool's
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

The **librpmem** utilizes **ssh**(1) client to login and execute the
**rpmemd**(1) process on remote node. By default the **ssh** process
is executed with **-4** option which forces using **IPv4** addressing.
The SSH command executed by **librpmem** can be overwritten by
**RPMEM_SSH** environment variable. The command executed by the **ssh**
can be overwritten by **RPMEM_CMD** variable. See **ENVIRONMENT**
section for details. See **FORK** section for more details.

# FORK #
The **ssh** process is executed
by **rpmem_open**() and **rpmem_create**() after forking a child process
using **fork**(2).  The application must take into account this fact when
using **wait**(2) and **waitpid**(2) functions which may return a PID of
the **ssh** process executed by **librpmem**.

The **librpmem** library calls the **ibv_fork_init**(3) function in library's
constructor in order to enable **fork**(2) support in **libibverbs**.
If an application uses the **libibverbs** before loading the **librpmem**
library it must call the **ibv_fork_inif**(3) function before allocating
any resources using **libibverbs**, otherwise **rpmem_open** and
**rpmem_create** functions will return an error.

# REMOTE POOL SIZE #
The remote pool size depends on the configuration of pool set file on remote
node. The remote pool size is a sum of sizes of all part files decreased by 4096
bytes per each part file. The 4096 bytes of each part file is utilized for
storing internal metadata of the pool part files. The minimum size of the
remote pool is 4096 bytes (not including required 4096 bytes per each part
file).

# LIBRARY API VERSIONING #

This section describes how the library API is versioned, allowing
applications to work with an evolving API.

```c
const char *rpmem_check_version(
	unsigned major_required,
	unsigned minor_required);
```

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


# DEBUGGING AND ERROR HANDLING #

Two versions of **librpmem** are typically available on a development
system. The normal version, accessed when a program is linked using the
**-lrpmem** option, is optimized for performance. That version skips
checks that impact performance and never logs any trace information or
performs any run-time assertions. If an error is detected during the
call to **librpmem** function, an application may retrieve an error
message describing the reason of failure using the following function:

```c
const char *rpmem_errormsg(void);
```

The **rpmem_errormsg**() function returns a pointer to a static buffer
containing the last error message logged for current thread. The error
message may include description of the corresponding error code (if
*errno* was set), as returned by **strerror**(3). The error message buffer
is thread-local; errors encountered in one thread do not affect its
value in other threads. The buffer is never cleared by any library
function; its content is significant only when the return value of the
immediately preceding call to **librpmem** function indicated an error,
or if *errno* was set. The application must not modify or free the error
message string, but it may be modified by subsequent calls to other
library functions.

A second version of **librpmem**, accessed when a program uses the
libraries under **/usr/lib/nvml_debug**, contains run-time assertions
and trace points. The typical way to access the debug version is to set
the environment variable **LD_LIBRARY_PATH** to
**/usr/lib/nvml_debug** or **/usr/lib64/nvml_debug** depending on
where the debug libraries are installed on the system. The trace points
in the debug version of the library are enabled using the environment
variable **RPMEM_LOG_LEVEL**, which can be set to the following values:

+ **0** - This is the default level when **RPMEM_LOG_LEVEL** is not set.
  No log messages are emitted at this level.

+ **1** - Additional details on any errors detected are logged (in addition
  to returning the *errno*-based errors as usual). The same information
  may be retrieved using **rpmem_errormsg**().

+ **2** - A trace of basic operations is logged.

+ **3** - This level enables a very verbose amount of function call
  tracing in the library.

+ **4** - This level enables voluminous and fairly obscure tracing
  information that is likely only useful to the **librpmem** developers.

The environment variable **RPMEM_LOG_FILE** specifies a file name where
all logging information should be written. If the last character in the
name is "-", the PID of the current process will be appended to the file
name when the log file is created. If **RPMEM_LOG_FILE** is not set,
the logging output goes to stderr.

Setting the environment variable **RPMEM_LOG_LEVEL** has no effect on
the non-debug version of **librpmem**.


# ENVIRONMENT #

**librpmem** can change its default behavior based on the following
environment variables. These are largely intended for testing and are
not normally required.

+ **RPMEM_CMD**=*cmd*

Setting this environment variable makes it possible to override the
default command executed on remote node using **ssh**. Setting this
variable shall not be required normally, but it can be used for testing
and debugging purposes.

**RPMEM_CMD** can contain multiple commands separated by vertical bar (`|`).
Each consecutive command is executed on remote node in order read from a
poolset file. This environment variable is read when library is initialized so
**RPMEM_CMD** must be set prior to application launch or prior to **dlopen**(3)
of **librpmem** in case of using dynamic linking loader.

+ **RPMEM_SSH**=*ssh_client*

Setting this environment variable makes it possible to override the
default **ssh** client command name. Setting this variable shall not
be required normally.

+ **RPMEM_ENABLE_SOCKETS**=0\|1

Setting this variable to 1 enables using **fi_sockets**(7) provider for
in-band RDMA connection. By default the *sockets* provider is disabled.

* **RPMEM_ENABLE_VERBS**=0\|1

Setting this variable to 0 disables using **fi_verbs**(7) provider for
in-band RDMA connection. The *verbs* provider is enabled by default.


# EXAMPLE #

The following example uses **librpmem** to create a remote pool on given
target node identified by given pool set name. The associated local memory
pool is zeroed and the data is made persistent on remote node. Upon success
the remote pool is closed.

```c
#include <stdio.h>
#include <string.h>

#include <librpmem.h>

#define POOL_SIZE    (32 * 1024 * 1024)
#define NLANES        4
unsigned char pool[POOL_SIZE];

int
main(int argc, char *argv[])
{
	int ret;
	unsigned nlanes = NLANES;

	/* fill pool_attributes */
	struct rpmem_pool_attr pool_attr;
	memset(&pool_attr, 0, sizeof(pool_attr));

	/* create a remote pool */
	RPMEMpool *rpp = rpmem_create("localhost", "pool.set",
			pool, POOL_SIZE, &nlanes, &pool_attr);
	if (!rpp) {
		fprintf(stderr, "rpmem_create: %s\n", rpmem_errormsg());
		return 1;
	}

	/* store data on local pool */
	memset(pool, 0, POOL_SIZE);

	/* make local data persistent on remote node */
	ret = rpmem_persist(rpp, 0, POOL_SIZE, 0);
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

	return 0;
}
```


# ACKNOWLEDGEMENTS #

**librpmem** builds on the persistent memory programming model
recommended by the SNIA NVM Programming Technical Work Group:
<http://snia.org/nvmp>


# SEE ALSO #

**libpmemobj**(3), **libpmemblk**(3), **libpmemlog**(3),
**libpmem**(3), **strerror**(3)
and **<http://pmem.io>**

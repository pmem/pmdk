---
layout: manual
Content-Style: 'text/css'
title: LIBPMEMCTO
collection: libpmemcto
header: PMDK
date: libpmemcto API version 1.0
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

[comment]: <> (libpmemcto.7 -- man page for libpmemcto)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[CAVEATS](#caveats)<br />
[LIBRARY API VERSIONING](#library-api-versioning-1)<br />
[MANAGING LIBRARY BEHAVIOR](#managing-library-behavior-1)<br />
[DEBUGGING AND ERROR HANDLING](#debugging-and-error-handling)<br />
[EXAMPLE](#example)<br />
[BUGS](#bugs)<br />
[NOTES](#notes)<br />
[AVAILABILITY](#availability)<br />
[ACKNOWLEDGEMENTS](#acknowledgements)<br />
[SEE ALSO](#see-also)<br />


# NAME #

**libpmemcto** -- close-to-open persistence


# SYNOPSIS #

```c
#include <libpmemcto.h>
cc ... -lpmemcto -lpmem
```


>NOTE: The PMDK API supports UNICODE. If the **PMDK_UTF8_API** macro is
defined, basic API functions are expanded to the UTF-8 API with postfix *U*.
Otherwise they are expanded to the UNICODE API with postfix *W*.

##### Library API versioning: #####

```c
const char *pmemcto_check_versionU(
	unsigned major_required,
	unsigned minor_required);
const wchar_t *pmemcto_check_versionW(
	unsigned major_required,
	unsigned minor_required);
```

##### Managing library behavior: #####

```c
void pmemcto_set_funcs(
	void *(*malloc_func)(size_t size),
	void (*free_func)(void *ptr),
	void *(*realloc_func)(void *ptr, size_t size),
	char *(*strdup_func)(const char *s),
	void (*print_func)(const char *s));
```

##### Error handling: #####

```c
const char *pmemcto_errormsgU(void);
const wchar_t *pmemcto_errormsgW(void);
```

##### Other library functions: #####

A description of other **libpmemcto** functions can be found on the following
manual pages:

**pmemcto_aligned_alloc**(3), **pmemcto_malloc**(3),
**pmemcto_malloc_usable_size**(3), **pmemcto_open**(3),
**pmemcto_set_root_pointer**(3), **pmemcto_stats_print**(3),
**pmemcto_strdup**(3), **pmemcto_wcsdup**(3)


# DESCRIPTION #

**libpmemcto** is a *persistent memory* allocator with no overhead imposed by
run-time flushing or transactional updates:

+ It runs at traditional **volatile** memory allocator speeds - there is
  no flushing or consistency check at run-time.

+ An overhead imposed only when program exits normally and have to flush
  the file.

+ The program flushes the pool contents when it exits, and then rebuilds
  the pool on the next run.

+ If the program crashes before flushing the file (or if flushing fails),
  the pool is in an inconsistent state causing subsequent pool opening to fail.

**libpmemcto** provides common *malloc-like* interfaces to persistent memory
pools built on memory-mapped files.  **libpmemcto** uses the **mmap**(2) system
call to create a pool of persistent memory.  The library is intended for
applications using *Direct Access* storage (DAX), which is memory-addressable
persistent storage that supports load/store access without being paged via
the system page cache.  A Persistent Memory-aware file system is typically used
to provide this type of access.  Memory-mapping a file from
a Persistent Memory-aware file system provides the raw memory pools, and this
library supplies the more familiar *malloc-like* interfaces on top
of those pools. **libpmemcto** is one of a collection of persistent memory
libraries available, the others are:

+ **libpmemobj**(7), a general use persistent memory API, providing memory
allocation and transactional operations on variable-sized objects.

+ **libpmemblk**(7), providing pmem-resident arrays of fixed-sized blocks with
atomic updates.

+ **libpmemlog**(7), providing a pmem-resident log file.

+ **libpmem**(7), low-level persistent memory support.

Under normal usage, **libpmemcto** will never print messages or intentionally
cause the process to exit.  Exceptions to this are prints caused by calls to
**pmemcto_stats_print**(3), or by enabling debugging as described under
**DEBUGGING AND ERROR HANDLING** below.  The library uses **pthreads**(7)
to be fully MT-safe, but never creates or destroys threads itself.
The library does not make use of any signals, networking, and never calls
**select**() or **poll**().

The system memory allocation routines like **malloc**() and **free**()
are used by **libpmemcto** for managing a small amount of run-time state,
but applications are allowed to override these calls if necessary
(see **pmemcto_set_funcs**()).

This library builds on the low-level pmem support provided by **libpmem**(7).

To use close-to-open persistence supplied by **libpmemcto**, a *memory pool*
is first created using the **pmemcto_createU**()/**pmemcto_createW**() function described
in **pmemcto_open**(3). The other **libpmemcto** functions operate on
the resulting block memory pool using the opaque handle, of type
*PMEMctopool\**, that is returned by **pmemcto_createU**()/**pmemcto_createW**() or **pmemcto_openU**()/**pmemcto_openW**().
Internally, **libpmemcto** will use either
**pmem_persist**(3) or **msync**(2) when it needs to flush changes, depending
on whether the memory pool appears to be persistent memory or a regular file
(see the **pmem_is_pmem**(3) function in **libpmem**(7) for more information).
There is no need for applications to flush changes directly when using the
close-to-open persistence memory API provided by **libpmemcto**.


# CAVEATS #

**libpmemcto** relies on the library destructor being called from the main
thread. For this reason, all functions that might trigger destruction (e.g.
**dlclose**(3)) should be called in the main thread. Otherwise some of the
resources associated with that thread might not be cleaned up properly.


# LIBRARY API VERSIONING #

This section describes how the library API is versioned,
allowing applications to work with an evolving API.

The **pmemcto_check_versionU**()/**pmemcto_check_versionW**() function is used to determine whether the
installed **libpmemcto** supports the version of the library API required
by an application. The easiest way to do this is for the application to
supply the compile-time version information, supplied by defines in
**\<ibpmemcto.h\>**, like this:

```c
reason = pmemcto_check_versionU(PMEMCTO_MAJOR_VERSION,
                               PMEMCTO_MINOR_VERSION);
if (reason != NULL) {
	/* version check failed, reason string tells you why */
}
```

Any mismatch in the major version number is considered a failure, but a library
with a newer minor version number will pass this check since increasing minor
versions imply backwards compatibility.

An application can also check specifically for the existence of an interface
by checking for the version where that interface was introduced. These versions
are documented in this man page as follows: unless otherwise specified, all
interfaces described here are available in version 1.0 of the library.
Interfaces added after version 1.0 will contain the text *introduced in
version x.y* in the section of this manual describing the feature.

When the version check performed by **pmemcto_check_versionU**()/**pmemcto_check_versionW**() is successful,
the return value is NULL. Otherwise the return value is a static string
describing the reason for failing the version check. The string returned by
**pmemcto_check_versionU**()/**pmemcto_check_versionW**() must not be modified or freed.


# MANAGING LIBRARY BEHAVIOR #

The **pmemcto_set_funcs**() function allows an application to override memory
allocation calls used internally by **libpmemcto**.
Passing in NULL for any of the handlers will cause
the **libpmemcto** default function to be used.
The library does not make heavy use of the system malloc functions,
but it does allocate approximately 4-8 kilobytes for each memory pool in use.


# DEBUGGING AND ERROR HANDLING #

The **pmemcto_errormsgU**()/**pmemcto_errormsgW**() function returns a pointer to a static buffer
containing the last error message logged for the current thread. If *errno* was
set, the error message may include a description of the corresponding error
code, as returned by **strerror**(3). The error message buffer is thread-local;
errors encountered in one thread do not affect its value in other threads. The
buffer is never cleared by any library function; its content is significant
only when the return value of the immediately preceding call to a
**libpmemcto** function indicated an error, or if *errno* was set. The
application must not modify or free the error message string, but it may be
modified by subsequent calls to other library functions.

Two versions of **libpmemcto** are typically available on a development system.
The normal version, accessed when a program is linked using the **-lpmemcto**
option, is optimized for performance. That version skips checks that impact
performance and never logs any trace information or performs any run-time
assertions. If an error is detected in a call to **libpmemcto**,
the error message describing the failure may be retrieved with
**pmemcto_errormsgU**()/**pmemcto_errormsgW**() as described above.

A second version of **libpmemcto**, accessed when a program uses the libraries
under **/pmdk/src/x64/Debug**, contains run-time assertions and trace points. The
typical way to access the debug version is to set the  **LD_LIBRARY_PATH**
environment variable to **/pmdk/src/x64/Debug**. Debugging output is
controlled using the following environment variables. These variables have
no effect on the non-debug version of the library.

+ **PMEMCTO_LOG_LEVEL**

The value of **PMEMCTO_LOG_LEVEL** enables trace points in the debug version
of the library, as follows:

+ **0** - This is the default level when **PMEMCTO_LOG_LEVEL** is not set.
No log messages are emitted at this level.

+ **1** - Additional details on any errors detected are logged,
in addition to returning the *errno*-based errors as usual.
The same information may be retrieved using **pmemcto_errormsgU**()/**pmemcto_errormsgW**().

+ **2** - A trace of basic operations is logged.

+ **3** - Enables a very verbose amount of function call tracing
in the library.

+ **4** - Enables voluminous and fairly obscure tracing information
that is likely only useful to the **libpmemcto** developers.

Unless **PMEMCTO_LOG_FILE** is set, debugging output is written to *stderr*.

+ **PMEMCTO_LOG_FILE**

Specifies the name of a file
where all logging information should be written. If the last character in the
name is "-", the *PID* of the current process will be appended to the file name
when the log file is created. If **PMEMCTO_LOG_FILE** is not set, the logging
output is written to *stderr*.

See also **libpmem**(7) for information on other environment variables
that may affect **libpmemcto** behavior.


# EXAMPLE #

The following example creates a memory pool, allocates some memory
to contain the string "hello, world", and then frees that memory.

```c
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <libpmemcto.h>

/* size of the pmemcto pool -- 1 GB */
#define POOL_SIZE ((size_t)(1 << 30))

/* name of our layout in the pool */
#define LAYOUT_NAME "example_layout"

struct root {
	char *str;
	char *data;
};

int
main(int argc, char *argv[])
{
	const char path[] = "/pmem-fs/myfile";
	PMEMctopool *pcp;

	/* create the pmemcto pool or open it if already exists */
	pcp = pmemcto_create(path, LAYOUT_NAME, POOL_SIZE, 0666);

	if (pcp == NULL)
		pcp = pmemcto_open(path, LAYOUT_NAME);

	if (pcp == NULL) {
		perror(path);
		exit(1);
	}

	/* get the root object pointer */
	struct root *rootp = pmemcto_get_root_pointer(pcp);

	if (rootp == NULL) {
		/* allocate root object */
		rootp = pmemcto_malloc(pcp, sizeof(*rootp));

		if (rootp == NULL) {
			perror(pmemcto_errormsg());
			exit(1);
		}

		/* save the root object pointer */
		pmemcto_set_root_pointer(pcp, rootp);

		rootp->str = pmemcto_strdup(pcp, "Hello World!");
		rootp->data = NULL;
	}

	/* ... */

	pmemcto_close(pcp);
}
```

See <http://pmem.io/pmdk/libpmemcto> for more examples using the
**libpmemcto** API.


# BUGS #

Unlike **libpmemobj**(3), data replication is not supported in **libpmemcto**.
Thus, it is not allowed to specify replica sections in pool set files.


# NOTES #

Unlike the normal **malloc**(), which asks the system for additional
memory when it runs out, **libpmemcto** allocates the size it is told
to and never attempts to grow or shrink that memory pool.


# AVAILABILITY #

**libpmemcto** is part of the PMDK since version 1.4 and is available
from <https://github.com/pmem/pmdk>


# ACKNOWLEDGEMENTS #

**libpmemcto** depends on jemalloc, written by Jason Evans, to do the heavy
lifting of managing dynamic memory allocation. See:
<http://www.canonware.com/jemalloc>

**libpmemcto** builds on the persistent memory programming model recommended
by the SNIA NVM Programming Technical Work Group:
<http://snia.org/nvmp>


# SEE ALSO #

**ndctl-create-namespace**(1), **dlclose**(2), **mmap**(2),
**jemalloc**(3), **malloc**(3),
**pmemcto_aligned_alloc**(3), **pmemcto_errormsg**(3), **pmemcto_malloc**(3),
**pmemcto_malloc_usable_size**(3), **pmemcto_open**(3),
**pmemcto_set_root_pointer**(3), **pmemcto_stats_print**(3),
**pmemcto_strdup**(3), **pmemcto_wcsdup**(3),
**libpmem**(7), **libpmemblk**(7), **libpmemlog**(7), **libpmemobj**(7)
and **<http://pmem.io>**

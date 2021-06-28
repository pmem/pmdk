---
layout: manual
Content-Style: 'text/css'
title: LIBPMEMBLK
collection: libpmemblk
header: PMDK
date: pmemblk API version 1.1
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2016-2018, Intel Corporation)

[comment]: <> (libpmemblk.7 -- man page for libpmemblk)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[LIBRARY API VERSIONING](#library-api-versioning-1)<br />
[MANAGING LIBRARY BEHAVIOR](#managing-library-behavior-1)<br />
[DEBUGGING AND ERROR HANDLING](#debugging-and-error-handling)<br />
[EXAMPLE](#example)<br />
[BUGS](#bugs)<br />
[ACKNOWLEDGEMENTS](#acknowledgements)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**libpmemblk** - persistent memory resident array of blocks

# SYNOPSIS #

```c
#include <libpmemblk.h>
cc ... -lpmemblk -lpmem
```


>NOTE: The PMDK API supports UNICODE. If the **PMDK_UTF8_API** macro is
defined, basic API functions are expanded to the UTF-8 API with postfix *U*.
Otherwise they are expanded to the UNICODE API with postfix *W*.

##### Library API versioning: #####

```c
const char *pmemblk_check_versionU(
	unsigned major_required,
	unsigned minor_required);
const wchar_t *pmemblk_check_versionW(
	unsigned major_required,
	unsigned minor_required);
```

##### Managing library behavior: #####

```c
void pmemblk_set_funcs(
	void *(*malloc_func)(size_t size),
	void (*free_func)(void *ptr),
	void *(*realloc_func)(void *ptr, size_t size),
	char *(*strdup_func)(const char *s));
```

##### Error handling: #####

```c
const char *pmemblk_errormsgU(void);
const wchar_t *pmemblk_errormsgW(void);
```

##### Other library functions: #####

A description of other **libpmemblk** functions can be found on the following
manual pages:

**pmemblk_bsize**(3), **pmemblk_create**(3),
**pmemblk_ctl_exec**(3), **pmemblk_ctl_get**(3), **pmemblk_ctl_set**(3),
**pmemblk_read**(3), **pmemblk_set_zero**(3),

# DESCRIPTION #

**libpmemblk**
provides an array of blocks in *persistent memory* (pmem) such that updates
to a single block are atomic. This library is intended for applications
using direct access storage (DAX), which is storage that supports load/store
access without paging blocks from a block storage device. Some types of
*non-volatile memory DIMMs* (NVDIMMs) provide this type of byte addressable
access to storage. A *persistent memory aware file system* is typically used
to expose the direct access to applications. Memory mapping a file from this
type of file system results in the load/store, non-paged access to pmem.
**libpmemblk** builds on this type of memory mapped file.

This library is for applications that need a potentially large array of blocks,
all the same size, where any given block is updated atomically (the update
cannot be *torn* by program interruption such as power failures). This library
builds on the low-level pmem support provided by **libpmem**(7), handling the
transactional update of the blocks, flushing to persistence, and recovery for
the application. **libpmemblk** is one of a collection of persistent memory
libraries available, the others are:

+ **libpmemobj**(7), a general use persistent memory API, providing memory
allocation and transactional operations on variable-sized objects.

+ **libpmemlog**(7), providing a pmem-resident log file.

+ **libpmem**(7), low-level persistent memory support.

Under normal usage, **libpmemblk** will never print messages or intentionally
cause the process to exit. The only exception to this is the debugging
information, when enabled, as described under **DEBUGGING AND ERROR HANDLING**
below.

To use the atomic block arrays supplied by **libpmemblk**, a *memory pool*
is first created using the **pmemblk_createU**()/**pmemblk_createW**() function described
in **pmemblk_create**(3). The other **libpmemblk** functions operate on
the resulting block memory pool using the opaque handle, of type
*PMEMblkpool\**, that is returned by **pmemblk_createU**()/**pmemblk_createW**() or **pmemblk_openU**()/**pmemblk_openW**().
Internally, **libpmemblk** will use either
**pmem_persist**(3) or **msync**(2) when it needs to flush changes, depending
on whether the memory pool appears to be persistent memory or a regular file
(see the **pmem_is_pmem**(3) function in **libpmem**(7) for more information).
There is no need for applications to flush changes directly when using the
block memory API provided by **libpmemblk**.

# CAVEATS #

**libpmemblk** relies on the library destructor being called from the main
thread. For this reason, all functions that might trigger destruction (e.g.
**dlclose**(3)) should be called in the main thread. Otherwise some of the
resources associated with that thread might not be cleaned up properly.

# LIBRARY API VERSIONING #

This section describes how the library API is versioned,
allowing applications to work with an evolving API.

The **pmemblk_check_versionU**()/**pmemblk_check_versionW**() function is used to determine whether the
installed **libpmemblk** supports the version of the library API required
by an application. The easiest way to do this is for the application to
supply the compile-time version information, supplied by defines in
**\<libpmemblk.h\>**, like this:

```c
reason = pmemblk_check_versionU(PMEMBLK_MAJOR_VERSION,
                               PMEMBLK_MINOR_VERSION);
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

When the version check performed by **pmemblk_check_versionU**()/**pmemblk_check_versionW**() is successful,
the return value is NULL. Otherwise the return value is a static string
describing the reason for failing the version check. The string returned by
**pmemblk_check_versionU**()/**pmemblk_check_versionW**() must not be modified or freed.

# MANAGING LIBRARY BEHAVIOR #

The **pmemblk_set_funcs**() function allows an application to override memory
allocation calls used internally by **libpmemblk**.
Passing in NULL for any of the handlers will cause
the **libpmemblk** default function to be used.
The library does not make heavy use of the system malloc functions,
but it does allocate approximately 4-8 kilobytes for each memory pool in use.

# DEBUGGING AND ERROR HANDLING #

The **pmemblk_errormsgU**()/**pmemblk_errormsgW**() function returns a pointer to a static buffer
containing the last error message logged for the current thread. If *errno* was
set, the error message may include a description of the corresponding error
code, as returned by **strerror**(3). The error message buffer is thread-local;
errors encountered in one thread do not affect its value in other threads. The
buffer is never cleared by any library function; its content is significant
only when the return value of the immediately preceding call to a
**libpmemblk** function indicated an error, or if *errno* was set. The
application must not modify or free the error message string, but it may be
modified by subsequent calls to other library functions.

Two versions of **libpmemblk** are typically available on a development system.
The normal version, accessed when a program is linked using the **-lpmemblk**
option, is optimized for performance. That version skips checks that impact
performance and never logs any trace information or performs any run-time
assertions. If an error is detected in a call to **libpmemblk**,
the error message describing the failure may be retrieved with
**pmemblk_errormsgU**()/**pmemblk_errormsgW**() as described above.

A second version of **libpmemblk**, accessed when a program uses the libraries
under **/pmdk/src/x64/Debug**, contains run-time assertions and trace points. The
typical way to access the debug version is to set the  **LD_LIBRARY_PATH**
environment variable to **/pmdk/src/x64/Debug**. Debugging output is
controlled using the following environment variables. These variables have
no effect on the non-debug version of the library.

+ **PMEMBLK_LOG_LEVEL**

The value of **PMEMBLK_LOG_LEVEL** enables trace points in the debug version
of the library, as follows:

+ **0** - This is the default level when **PMEMBLK_LOG_LEVEL** is not set.
No log messages are emitted at this level.

+ **1** - Additional details on any errors detected are logged,
in addition to returning the *errno*-based errors as usual.
The same information may be retrieved using **pmemblk_errormsgU**()/**pmemblk_errormsgW**().

+ **2** - A trace of basic operations is logged.

+ **3** - Enables a very verbose amount of function call tracing
in the library.

+ **4** - Enables voluminous and fairly obscure tracing information
that is likely only useful to the **libpmemblk** developers.

Unless **PMEMBLK_LOG_FILE** is set, debugging output is written to *stderr*.

+ **PMEMBLK_LOG_FILE**

Specifies the name of a file
where all logging information should be written. If the last character in the
name is "-", the *PID* of the current process will be appended to the file name
when the log file is created. If **PMEMBLK_LOG_FILE** is not set, the logging
output is written to *stderr*.

See also **libpmem**(7) for information on other environment variables
that may affect **libpmemblk** behavior.

# EXAMPLE #

The following example illustrates how the **libpmemblk** API is used.

```c
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <libpmemblk.h>

/* size of the pmemblk pool -- 1 GB */
#define POOL_SIZE ((size_t)(1 << 30))

/* size of each element in the pmem pool */
#define ELEMENT_SIZE 1024

int
main(int argc, char *argv[])
{
	const char path[] = "/pmem-fs/myfile";
	PMEMblkpool *pbp;
	size_t nelements;
	char buf[ELEMENT_SIZE];

	/* create the pmemblk pool or open it if it already exists */
	pbp = pmemblk_createU(path, ELEMENT_SIZE, POOL_SIZE, 0666);

	if (pbp == NULL)
		pbp = pmemblk_openU(path, ELEMENT_SIZE);

	if (pbp == NULL) {
		perror(path);
		exit(1);
	}

	/* how many elements fit into the file? */
	nelements = pmemblk_nblock(pbp);
	printf("file holds %zu elements", nelements);

	/* store a block at index 5 */
	strcpy(buf, "hello, world");
	if (pmemblk_write(pbp, buf, 5) < 0) {
		perror("pmemblk_write");
		exit(1);
	}

	/* read the block at index 10 (reads as zeros initially) */
	if (pmemblk_read(pbp, buf, 10) < 0) {
		perror("pmemblk_read");
		exit(1);
	}

	/* zero out the block at index 5 */
	if (pmemblk_set_zero(pbp, 5) < 0) {
		perror("pmemblk_set_zero");
		exit(1);
	}

	/* ... */

	pmemblk_close(pbp);
}
```

See <https://pmem.io/pmdk/libpmemblk> for more examples using the **libpmemblk** API.

# BUGS #

Unlike **libpmemobj**(7), data replication is not supported in **libpmemblk**.
Thus, specifying replica sections in pool set files is not allowed.

# ACKNOWLEDGEMENTS #

**libpmemblk** builds on the persistent memory programming model recommended
by the SNIA NVM Programming Technical Work Group:
<https://snia.org/nvmp>

# SEE ALSO #

**msync**(2), **dlclose**(3), **pmemblk_bsize**(3), **pmemblk_create**(3),
**pmemblk_ctl_exec**(3), **pmemblk_ctl_get**(3), **pmemblk_ctl_set**(3),
**pmemblk_read**(3), **pmemblk_set_zero**(3), **pmem_is_pmem**(3),
**pmem_persist**(3), **strerror**(3), **libpmem**(7),
**libpmemlog**(7), **libpmemobj**(7) and **<https://pmem.io>**

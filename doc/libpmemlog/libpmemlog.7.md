---
draft: false
slider_enable: true
description: ""
disclaimer: "The contents of this web site and the associated <a href=\"https://github.com/pmem\">GitHub repositories</a> are BSD-licensed open source."
aliases: ["libpmemlog.7.html"]
title: "libpmemlog | PMDK"
header: "pmemlog API version 1.1"
---

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2016-2023, Intel Corporation)

[comment]: <> (libpmemlog.7 -- man page for libpmemlog)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[CAVEATS](#caveats)<br />
[LIBRARY API VERSIONING](#library-api-versioning-1)<br />
[MANAGING LIBRARY BEHAVIOR](#managing-library-behavior-1)<br />
[DEBUGGING AND ERROR HANDLING](#debugging-and-error-handling)<br />
[EXAMPLE](#example)<br />
[BUGS](#bugs)<br />
[ACKNOWLEDGEMENTS](#acknowledgements)<br />
[SEE ALSO](#see-also)

# NAME #

**libpmemlog** - persistent memory resident log file (DEPRECATED)

>NOTE:
Support for Windows and FreeBSD deprecated since PMDK 1.13.0 release
and will be removed in the PMDK 2.0.0 release.

# SYNOPSIS #

```c
#include <libpmemlog.h>
cc ... -lpmemlog -lpmem
```

_UNICODE()

##### Library API versioning: #####

```c
_UWFUNC(pmemlog_check_version, =q=
	unsigned major_required,
	unsigned minor_required=e=)
```

##### Managing library behavior: #####

```c
void pmemlog_set_funcs(
	void *(*malloc_func)(size_t size),
	void (*free_func)(void *ptr),
	void *(*realloc_func)(void *ptr, size_t size),
	char *(*strdup_func)(const char *s));
```

##### Error handling: #####

```c
_UWFUNCR(int, pmemlog_check, *path)
```

##### Other library functions: #####

A description of other **libpmemlog** functions can be found on the following
manual pages:

**pmemlog_append**(3), **pmemlog_create**(3), **pmemlog_ctl_exec**(3),
**pmemlog_ctl_get**(3), **pmemlog_ctl_set**(3), **pmemlog_nbyte**(3),
**pmemlog_tell**(3)

# DESCRIPTION #

**libpmemlog**
provides a log file in *persistent memory* (pmem) such that
additions to the log are appended atomically. This library is intended
for applications using direct access storage (DAX), which is storage
that supports load/store access without paging blocks from a block
storage device. Some types of *non-volatile memory DIMMs* (NVDIMMs) provide
this type of byte addressable access to storage. A *persistent memory aware
file system* is typically used to expose the direct access to applications.
Memory mapping a file from this type of file system
results in the load/store, non-paged access to pmem.
**libpmemlog** builds on thistype of memory mapped file.

This library is for applications that need a persistent log file
updated atomically (the updates cannot be *torn* by program interruption
such as power failures). This library builds on the low-level pmem
support provided by **libpmem**(7), handling the transactional update of
the log, flushing to persistence, and recovery for the application.

**libpmemlog** is one of a collection of persistent memory libraries available.
The others are:

+ **libpmemobj**(7), a general use persistent memory API,
	providing memory allocation and transactional operations on variable-sized objects.

+ **libpmemblk**(7), providing pmem-resident arrays of fixed-sized blocks with atomic updates.

+ **libpmem**(7), low-level persistent memory support.

Under normal usage, **libpmemlog** will never print messages or intentionally
cause the process to exit. The only exception to this is the debugging
information, when enabled, as described under **DEBUGGING AND ERROR HANDLING** below.

To use the pmem-resident log file provided by **libpmemlog**, a *memory pool* is
first created. This is done with the **pmemlog_create**(3) function.
The other functions mentioned above in SYNOPSIS section
then operate on the resulting log memory pool.

Once created, the memory pool is represented by an opaque handle,
of type *PMEMlogpool\**, which is passed to most of the other
functions from **libpmemlog**. Internally, **libpmemlog** will use
either **pmem_persist**(3) or **msync**(2) when it needs to flush changes,
depending on whether the memory pool appears to be persistent memory
or a regular file (see the **pmem_is_pmem**(3) function in **libpmem**(7)
for more information). There is no need for applications to flush
changes directly when using the log memory API provided by **libpmemlog**.

# CAVEATS #

**libpmemlog** relies on the library destructor being called from the main
thread. For this reason, all functions that might trigger destruction (e.g.
**dlclose**(3)) should be called in the main thread. Otherwise some of the
resources associated with that thread might not be cleaned up properly.

# LIBRARY API VERSIONING #

This section describes how the library API is versioned,
allowing applications to work with an evolving API.

The _UW(pmemlog_check_version) function is used to determine whether the
installed **libpmemlog** supports the version of the library API required by
an application. The easiest way to do this is for the application to supply
the compile-time version information provided by defines in
**\<libpmemlog.h\>**, like this:

```c
reason = _U(pmemlog_check_version)(PMEMLOG_MAJOR_VERSION,
                               PMEMLOG_MINOR_VERSION);
if (reason != NULL) {
	/* version check failed, reason string tells you why */
}
```

Any mismatch in the major version number is considered a failure,
but a library with a newer minor version number will pass this check
since increasing minor versions imply backwards compatibility.

An application can also check specifically for the existence of an interface
by checking for the version where that interface was introduced. These versions
are documented in this man page as follows: unless otherwise specified, all
interfaces described here are available in version 1.0 of the library. Interfaces
added after version 1.0 will contain the text *introduced
in version x.y* in the section of this manual describing the feature.

On success, _UW(pmemlog_check_version) returns NULL. Otherwise, the return
value is a static string describing the reason the version check failed. The
string returned by _UW(pmemlog_check_version) must not be modified or freed.

# MANAGING LIBRARY BEHAVIOR #

The **pmemlog_set_funcs**() function allows an application to override
memory allocation calls used internally by **libpmemlog**.
Passing in NULL for any of the handlers will cause the
**libpmemlog** default function to be used. The library does not make
heavy use of the system malloc functions, but it does
allocate approximately 4-8 kilobytes for each memory pool in use.

# DEBUGGING AND ERROR HANDLING #

The _UW(pmemlog_errormsg) function returns a pointer to a static buffer
containing the last error message logged for the current thread. If *errno*
was set, the error message may include a description of the corresponding
error code as returned by **strerror**(3). The error message buffer is
thread-local; errors encountered in one thread do not affect its value in
other threads. The buffer is never cleared by any library function; its
content is significant only when the return value of the immediately preceding
call to a **libpmemlog** function indicated an error, or if *errno* was set.
The application must not modify or free the error message string, but it may
be modified by subsequent calls to other library functions.

Two versions of **libpmemlog** are typically available on a development
system. The normal version, accessed when a program is linked using the
**-lpmemlog** option, is optimized for performance. That version skips checks
that impact performance and never logs any trace information or performs any
run-time assertions.

A second version of **libpmemlog**, accessed when a program uses the libraries
under _DEBUGLIBPATH(), contains run-time assertions and trace points. The
typical way to access the debug version is to set the environment variable
**LD_LIBRARY_PATH** to _LDLIBPATH(). Debugging output is
controlled using the following environment variables. These variables have
no effect on the non-debug version of the library.

+ **PMEMLOG_LOG_LEVEL**

The value of **PMEMLOG_LOG_LEVEL** enables trace points in the debug version
of the library, as follows:

+ **0** - This is the default level when **PMEMLOG_LOG_LEVEL** is not set.
No log messages are emitted at this level.

+ **1** - Additional details on any errors detected are logged,
in addition to returning the *errno*-based errors as usual.
The same information may be retrieved using _UW(pmemlog_errormsg).

+ **2** - A trace of basic operations is logged.

+ **3** - Enables a very verbose amount of function call tracing in the library.

+ **4** - Enables voluminous and fairly obscure tracing information
that is likely only useful to the **libpmemlog** developers.

Unless **PMEMLOG_LOG_FILE** is set, debugging output is written to *stderr*.

+ **PMEMLOG_LOG_FILE**

Specifies the name of a file name where all logging information should be
written. If the last character in the name is "-", the *PID* of the current
process will be appended to the file name when the log file is created. If
**PMEMLOG_LOG_FILE** is not set, logging output is written to *stderr*.

See also **libpmem**(7) for information about other environment
variables affecting **libpmemlog** behavior.

# EXAMPLE #

The following example illustrates how the **libpmemlog** API is used.

```c
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <libpmemlog.h>

/* size of the pmemlog pool -- 1 GB */
#define POOL_SIZE ((size_t)(1 << 30))

/*
 * printit -- log processing callback for use with pmemlog_walk()
 */
int
printit(const void *buf, size_t len, void *arg)
{
	fwrite(buf, len, 1, stdout);
	return 0;
}

int
main(int argc, char *argv[])
{
	const char path[] = "/pmem-fs/myfile";
	PMEMlogpool *plp;
	size_t nbyte;
	char *str;

	/* create the pmemlog pool or open it if it already exists */
	plp = _U(pmemlog_create)(path, POOL_SIZE, 0666);

	if (plp == NULL)
		plp = _U(pmemlog_open)(path);

	if (plp == NULL) {
		perror(path);
		exit(1);
	}

	/* how many bytes does the log hold? */
	nbyte = pmemlog_nbyte(plp);
	printf("log holds %zu bytes", nbyte);

	/* append to the log... */
	str = "This is the first string appended";
	if (pmemlog_append(plp, str, strlen(str)) < 0) {
		perror("pmemlog_append");
		exit(1);
	}
	str = "This is the second string appended";
	if (pmemlog_append(plp, str, strlen(str)) < 0) {
		perror("pmemlog_append");
		exit(1);
	}

	/* print the log contents */
	printf("log contains:");
	pmemlog_walk(plp, 0, printit, NULL);

	pmemlog_close(plp);
}
```

See <https://pmem.io/pmdk/libpmemlog>
for more examples using the **libpmemlog** API.

# BUGS #

Unlike **libpmemobj**(7), data replication is not supported in **libpmemlog**.
Thus, specifying replica sections in pool set files is not allowed.

# NOTE #

> NOTICE:
The **libpmemlog** library is deprecated since PMDK 1.13.0 release
and will be removed in the PMDK 2.0.0 release.

# ACKNOWLEDGEMENTS #

**libpmemlog** builds on the persistent memory programming model recommended
by the SNIA NVM Programming Technical Work Group:
<https://snia.org/nvmp>

# SEE ALSO #

**msync**(2), **pmemlog_append**(3), **pmemlog_create**(3),
**pmemlog_ctl_exec**(3), **pmemlog_ctl_get**(3), **pmemlog_ctl_set**(3),
**pmemlog_nbyte**(3), **pmemlog_tell**(3), **strerror**(3),
**libpmem**(7), **libpmemblk**(7), **libpmemobj**(7)
and **<https://pmem.io>**

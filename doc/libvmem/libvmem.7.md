---
layout: manual
Content-Style: 'text/css'
title: _MP(LIBVMEM, 7)
collection: libvmem
header: PMDK
date: vmem API version 1.1
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

[comment]: <> (libvmem.7 -- man page for libvmem)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[MANAGING LIBRARY BEHAVIOR](#managing-library-behavior)<br />
[DEBUGGING AND ERROR HANDLING](#debugging-and-error-handling)<br />
[EXAMPLE](#example)<br />
[BUGS](#bugs)<br />
[ACKNOWLEDGEMENTS](#acknowledgements)<br />
[SEE ALSO](#see-also)<br />


# NAME #

**libvmem** - volatile memory allocation library


# SYNOPSIS #

```c
#include <libvmem.h>
cc ... -lvmem
```

_UNICODE()

##### Managing overall library behavior: #####

```c
_UWFUNC(vmem_check_version, =q=
	unsigned major_required,
	unsigned minor_required=e=)

void vmem_set_funcs(
	void *(*malloc_func)(size_t size),
	void (*free_func)(void *ptr),
	void *(*realloc_func)(void *ptr, size_t size),
	char *(*strdup_func)(const char *s),
	void (*print_func)(const char *s));
```

##### Error handling: #####

```c
_UWFUNC(vmem_errormsg, void)
```

##### Other library functions: #####

A description of other **libvmem** functions can be found on the following
manual pages:

+ memory pool management: **vmem_create**(3)

+ memory allocation related functions: **vmem_malloc**(3)


# DESCRIPTION #

**libvmem** provides common *malloc*-like interfaces to memory pools built on
memory-mapped files. These interfaces are for traditional **volatile** memory
allocation but, unlike the functions described in **malloc**(3),
the memory managed by **libvmem** may have different attributes,
depending on the file system containing the memory-mapped files.
In particular, **libvmem** is part of the *Persistent Memory Development Kit*
because it is sometimes useful to use non-volatile memory as a volatile memory
pool, leveraging its capacity, cost, or performance characteristics.

It is recommended that new code uses **memkind**(3) instead of **libvmem**, as
this library is no longer actively developed and lacks certain features of
**memkind** such as NUMA awareness.  Nevertheless, it is mature, and is
expected to be maintained for foreseable future.

**libvmem** uses the **mmap**(2) system call to create a pool of volatile
memory. The library is most useful when used with *Direct Access* storage
(DAX), which is memory-addressable persistent storage that supports load/store
access without being paged via the system page cache. A Persistent Memory-aware
file system is typically used to provide this type of access. Memory-mapping a
file from a Persistent Memory-aware file system provides the raw memory pools,
and this library supplies the more familiar *malloc*-like interfaces on top
of those pools.

Under normal usage, **libvmem** will never print messages or intentionally
cause the process to exit. Exceptions to this are prints caused by calls to
**vmem_stats_print**(3), or by enabling debugging as described under
**DEBUGGING AND ERROR HANDLING** below. The library uses **pthreads** to be
fully MT-safe, but never creates or destroys threads itself. The library does
not make use of any signals, networking, and never calls **select**(2) or
**poll**(2). The system memory allocation routines like **malloc**(3) and
**free**(3) are used by **libvmem** for managing a small amount of run-time
state, but applications are allowed to override these calls if necessary
(see the description of **vmem_set_funcs**() below).

**libvmem** interfaces are grouped into three categories: those that manage
memory pools, those providing the basic memory allocation functions, and
those interfaces less commonly used for managing the overall library behavior.


# MANAGING LIBRARY BEHAVIOR #

The _UW(vmem_check_version) function is used to see if the installed
**libvmem** supports the version of the library API required by an application.
The easiest way to do this is for the application to supply the compile-time
version information, supplied by defines in **\<libvmem.h\>**, like this:

```c
reason = _U(vmem_check_version)(VMEM_MAJOR_VERSION,
                            VMEM_MINOR_VERSION);
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
interfaces described here are available in version 1.0 of the library. Interfaces
added after version 1.0 will contain the text *introduced in version x.y*
in the section of this manual describing the feature.

When the version check is successful, _UW(vmem_check_version) returns NULL.
Otherwise, _UW(vmem_check_version) returns a static string describing the
reason for failing the version check. The returned string must not be modified
or freed.

The **vmem_set_funcs**() function allows an application to override some
interfaces used internally by **libvmem**. Passing NULL for any of the
handlers will cause the **libvmem** default function to be used. The only
functions in the malloc family used by the library are represented by the
first four arguments to **vmem_set_funcs**(). While the library does not make
heavy use of the system malloc functions, it does allocate approximately 4-8
kilobytes for each memory pool in use. The *print_func* function is called by
**libvmem** when the **vmem_stats_print**() entry point is used, or when
additional tracing is enabled in the debug version of the library as described
in **DEBUGGING AND ERROR HANDLING**, below. The default *print_func*
used by the library prints to the file specified by the **VMEM_LOG_FILE**
environment variable, or to *stderr* if that variable is not set.


# CAVEATS #

**libvmem** relies on the library destructor being called from the main thread.
For this reason, all functions that might trigger destruction (e.g.
**dlclose**(3)) should be called in the main thread. Otherwise some of the
resources associated with that thread might not be cleaned up properly.


# DEBUGGING AND ERROR HANDLING #

If an error is detected during the call to a **libvmem** function, the
application may retrieve an error message describing the reason for the failure
from _UW(vmem_errormsg). This function returns a pointer to a static buffer
containing the last error message logged for the current thread. If *errno*
was set, the error message may include a description of the corresponding
error code as returned by **strerror**(3). The error message buffer is
thread-local; errors encountered in one thread do not affect its value in
other threads. The buffer is never cleared by any library function; its
content is significant only when the return value of the immediately preceding
call to a **libvmem** function indicated an error, or if *errno* was set.
The application must not modify or free the error message string, but it may
be modified by subsequent calls to other library functions.

Two versions of **libvmem** are typically available on a development
system. The normal version is optimized for performance. That version skips
checks that impact performance and never logs any trace information or
performs any run-time assertions. A second version, accessed when using
libraries from _DEBUGLIBPATH(), contains run-time assertions and trace
points. The typical way to access the debug version is to set the
**LD_LIBRARY_PATH** environment variable to _LDLIBPATH(). Debugging output is
controlled using the following environment variables. These variables have
no effect on the non-debug version of the library.

+ **VMEM_LOG_LEVEL**

The value of **VMEM_LOG_LEVEL** enables trace points in the debug version
of the library, as follows:

+ **0** - Tracing is disabled. This is the default level when
**VMEM_LOG_LEVEL** is not set. Only statistics are logged, and then only in response to a call to **vmem_stats_print**().

+ **1** - Additional details on any errors detected are logged, in addition to
returning the *errno*-based errors as usual.

+ **2** - A trace of basic operations is logged.

+ **3** - Enables a very verbose amount of function call tracing
in the library.

+ **4** - Enables voluminous tracing information about all memory
allocations and deallocations.

Unless **VMEM_LOG_FILE** is set, debugging output is written to *stderr*.

+ **VMEM_LOG_FILE**

Specifies the name of a file where
all logging information should be written. If the last character in the name
is "-", the *PID* of the current process will be appended to the file name when
the log file is created. If **VMEM_LOG_FILE** is not set, output is
written to *stderr*.


# EXAMPLE #

The following example creates a memory pool, allocates some memory
to contain the string "hello, world", and then frees that memory.

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libvmem.h>

int
main(int argc, char *argv[])
{
	VMEM *vmp;
	char *ptr;

	/* create minimum size pool of memory */
	if ((vmp = _U(vmem_create)("/pmem-fs",
			VMEM_MIN_POOL)) == NULL) {
		perror("_U(vmem_create)");
		exit(1);
	}

	if ((ptr = vmem_malloc(vmp, 100)) == NULL) {
		perror("vmem_malloc");
		exit(1);
	}

	strcpy(ptr, "hello, world");

	/* give the memory back */
	vmem_free(vmp, ptr);

	/* ... */

	vmem_delete(vmp);
}
```

See <http://pmem.io/pmdk/libvmem> for more examples using the **libvmem** API.


# BUGS #

Unlike the normal **malloc**(3), which asks the system for additional
memory when it runs out, **libvmem** allocates the size it is told
to and never attempts to grow or shrink that memory pool.


# ACKNOWLEDGEMENTS #

**libvmem** depends on jemalloc, written by Jason Evans, to do the heavy
lifting of managing dynamic memory allocation. See:
<http://www.canonware.com/jemalloc>

**libvmem** builds on the persistent memory programming model recommended
by the SNIA NVM Programming Technical Work Group:
<http://snia.org/nvmp>


# SEE ALSO #

**mmap**(2), **dlclose**(3), **malloc**(3),
**strerror**(3), **vmem_create**(3), **vmem_malloc**(3),
and **<http://pmem.io>**

On Linux:

**jemalloc**(3), **pthreads**(7)

On FreeBSD:

**pthread**(3)

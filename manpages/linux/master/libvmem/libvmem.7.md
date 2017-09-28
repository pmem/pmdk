---
layout: manual
Content-Style: 'text/css'
title: LIBVMEM!7
collection: libvmem
header: NVM Library
date: vmem API version 1.1
...

[comment]: <> (Copyright 2016-2017, Intel Corporation)

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

**libvmem** -- volatile memory allocation library


# SYNOPSIS #

```c
#include <libvmem.h>
cc ... -lvmem
```



##### Memory pool management: #####

```c
VMEM *vmem_create(const char *dir, size_t size);
VMEM *vmem_create_in_region(void *addr, size_t size);
void vmem_delete(VMEM *vmp);
int vmem_check(VMEM *vmp);
void vmem_stats_print(VMEM *vmp, const char *opts);
```

##### Memory allocation related functions: #####

```c
void *vmem_malloc(VMEM *vmp, size_t size);
void vmem_free(VMEM *vmp, void *ptr);
void *vmem_calloc(VMEM *vmp, size_t nmemb, size_t size);
void *vmem_realloc(VMEM *vmp, void *ptr, size_t size);
void *vmem_aligned_alloc(VMEM *vmp, size_t alignment, size_t size);
char *vmem_strdup(VMEM *vmp, const char *s);
wchar_t *vmem_wcsdup(VMEM *vmp, const wchar_t *s);
size_t vmem_malloc_usable_size(VMEM *vmp, void *ptr);
```

##### Managing overall library behavior: #####

```c
const char *vmem_check_version(
	unsigned major_required,
	unsigned minor_required);
void vmem_set_funcs(
	void *(*malloc_func)(size_t size),
	void (*free_func)(void *ptr),
	void *(*realloc_func)(void *ptr, size_t size),
	char *(*strdup_func)(const char *s),
	void (*print_func)(const char *s));
```

##### Error handling: #####

```c
const char *vmem_errormsg(void);
```


# DESCRIPTION #

**libvmem** provides common *malloc-like* interfaces to memory pools built on
memory-mapped files. These interfaces are for traditional **volatile** memory
allocation but, unlike the functions described in **malloc**(3),
the memory managed by **libvmem** may have different attributes,
depending on the file system containing the memory-mapped files.
In particular, **libvmem** is part of the *Non-Volatile Memory Library*
because it is sometimes useful to use non-volatile memory as a volatile memory pool,
leveraging its capacity, cost, or performance characteristics.

**libvmem** uses the **mmap**(2) system call to create a pool of volatile memory.
The library is most useful when used with *Direct Access* storage (DAX),
which is memory-addressable persistent storage that supports load/store access
without being paged via the system page cache. A Persistent Memory-aware file
system is typically used to provide this type of access. Memory-mapping a file
from a Persistent Memory-aware file system provides the raw memory pools, and
this library supplies the more familiar *malloc-like* interfaces on top of those pools.

Under normal usage, **libvmem** will never print messages or intentionally
cause the process to exit. Exceptions to this are prints caused by calls to
**vmem_stats_print**(3), or by enabling debugging as described under
**DEBUGGING AND ERROR HANDLING** below. The library uses **pthreads**(7) to be fully
MT-safe, but never creates or destroys threads itself. The library does not make
use of any signals, networking, and never calls **select**() or **poll**().
The system memory allocation routines like **malloc**() and **free**() are used by
**libvmem** for managing a small amount of run-time state, but applications
are allowed to override these calls if necessary (see the description of
**vmem_set_funcs**() below).

**libvmem** interfaces are grouped into three categories:
those that manage memory pools, those providing the basic memory allocation functions,
and those interfaces less commonly used for managing the overall library behavior.


# MANAGING LIBRARY BEHAVIOR #

The library entry points described in this section are less commonly
used than the previous section. These entry points expose library
information or alter the default library behavior.


```c
const char *vmem_check_version(
	unsigned major_required,
	unsigned minor_required);
```

The **vmem_check_version**() function is used to see if the installed **libvmem**
supports the version of the library API required by an application.
The easiest way to do this is for the application to supply the compile-time
version information, supplied by defines in **\<libvmem.h\>**, like this:

```c
reason = vmem_check_version(VMEM_MAJOR_VERSION,
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

When the version check performed by **vmem_check_version**() is successful,
the return value is NULL. Otherwise the return value is a static string
describing the reason for failing the version check.
The string returned by **vmem_check_version**() must not be modified or freed.

```c
void vmem_set_funcs(
	void *(*malloc_func)(size_t size),
	void (*free_func)(void *ptr),
	void *(*realloc_func)(void *ptr, size_t size),
	char *(*strdup_func)(const char *s),
	void (*print_func)(const char *s));
```

The **vmem_set_funcs**() function allows an application to override some interfaces
used internally by **libvmem**. Passing in NULL for any of the handlers
will cause the **libvmem** default function to be used. The library does not make
heavy use of the system malloc functions, but it does allocate approximately
4-8 kilobytes for each memory pool in use. The only functions in the malloc
family used by the library are represented by the first four arguments to
**vmem_set_funcs**(). The *print_func* function is called by **libvmem** when
the **vmem_stats_print**() entry point is used, or when additional tracing is
enabled in the debug version of the library as described in the **DEBUGGING AND
ERROR HANDLING** section below. The default *print_func* used by the library
prints to the file specified by the **VMEM_LOG_FILE** environment variable,
or to *stderr* if that variable is not set.


# CAVEATS #

**libvmem** relies on the library destructor being called from the main thread.
For this reason, all functions that might trigger destruction (e.g.
**dlclose**()) should be called in the main thread. Otherwise some of the
resources associated with that thread might not be cleaned up properly.


# DEBUGGING AND ERROR HANDLING #

Two versions of **libvmem** are typically available on a development system.
The normal version, accessed when a program is linked using the **-lvmem** option,
is optimized for performance. That version skips checks that impact performance
and never logs any trace information or performs any run-time assertions. If an
error is detected during the call to **libvmem** function, an application may
retrieve an error message describing the reason of failure using the following
function:

```c
const char *vmem_errormsg(void);
```

The **vmem_errormsg**() function returns a pointer to a static buffer containing the
last error message logged for current thread. The error message may
include description of the corresponding error code (if *errno* was set),
as returned by **strerror**(3). The error message buffer is thread-local; errors
encountered in one thread do not affect its value in other threads. The buffer
is never cleared by any library function; its content is significant only when
the return value of the immediately preceding call to **libvmem** function
indicated an error, or if *errno* was set. The application must not modify or free
the error message string, but it may be modified by subsequent calls to other
library functions.

A second version of **libvmem**, accessed when a program uses
the libraries under **/usr/lib/nvml_debug**,
contains run-time assertions and trace points. The typical way to
access the debug version is to set the environment variable
**LD_LIBRARY_PATH** to **/usr/lib/nvml_debug** or **/usr/lib64/nvml_debug** depending on where the debug
libraries are installed on the system.
The trace points in the debug version of the library are enabled using the environment variable
**VMEM_LOG_LEVEL**, which can be set to the following values:

+ **0** - This is the default level when **VMEM_LOG_LEVEL** is not set.
	Only statistics are logged, and then only in response to a call to **vmem_stats_print**().

+ **1** - Additional details on any errors detected are logged
	(in addition to returning the *errno*-based errors as usual).
	The same information may be retrieved using **vmem_errormsg**().

+ **2** - A trace of basic operations including allocations and deallocations is logged.

+ **3** - This level enables a very verbose amount of function call tracing in the library.

+ **4** - This level enables voluminous and fairly obscure tracing information
	that is likely only useful to the **libvmem** developers.

The environment variable **VMEM_LOG_FILE** specifies a file name where all logging
information should be written. If the last character in the name is "-", the
PID of the current process will be appended to the file name when the log file
is created. If **VMEM_LOG_FILE** is not set, output goes to stderr. All prints are
done using the *print_func* function in **libvmem** (see **vmem_set_funcs**()
above for details on how to override that function).

Setting the environment variable **VMEM_LOG_LEVEL** has no effect on the
non-debug version of **libvmem**.


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
	if ((vmp = vmem_create("/pmem-fs",
			VMEM_MIN_POOL)) == NULL) {
		perror("vmem_create");
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

See <http://pmem.io/nvml/libvmem> for more examples using the **libvmem** API.


# BUGS #

Unlike the normal **malloc**(), which asks the system for additional
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

**malloc**(3), **mmap**(2), **strerror**(3),
**jemalloc**(3), **pthreads**(7) and **<http://pmem.io>**

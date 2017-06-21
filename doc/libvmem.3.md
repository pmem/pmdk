---
layout: manual
Content-Style: 'text/css'
title: LIBVMEM!3
header: NVM Library
date: vmem API version 1.0
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

[comment]: <> (libvmem.3 -- man page for libvmem)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[MANAGING MEMORY POOLS](#managing-memory-pools)<br />
[MEMORY ALLOCATION](#memory-allocation)<br />
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

!ifdef{WIN32}
{
>NOTE: NVML API supports UNICODE. If **NVML_UTF8_API** macro is defined then
basic API functions are expanded to UTF-8 API with postfix *U*,
otherwise they are expanded to UNICODE API with postfix *W*.
}

##### Memory pool management: #####

```c
!ifdef{WIN32}
{
VMEM *vmem_createU(const char *dir, size_t size);
VMEM *vmem_createW(const wchar_t *dir, size_t size);
}{
VMEM *vmem_create(const char *dir, size_t size);
}
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
!ifdef{WIN32}
{
const char *vmem_check_versionU(
	unsigned major_required,
	unsigned minor_required);
const wchar_t *vmem_check_versionW(
	unsigned major_required,
	unsigned minor_required);
}{
const char *vmem_check_version(
	unsigned major_required,
	unsigned minor_required);
}
void vmem_set_funcs(
	void *(*malloc_func)(size_t size),
	void (*free_func)(void *ptr),
	void *(*realloc_func)(void *ptr, size_t size),
	char *(*strdup_func)(const char *s),
	void (*print_func)(const char *s));
```

##### Error handling: #####

```c
!ifdef{WIN32}
{
const char *vmem_errormsgU(void);
const wchar_t *vmem_errormsgW(void);
}{
const char *vmem_errormsg(void);
}
```


# DESCRIPTION #

**libvmem** provides common *malloc-like* interfaces to memory pools built on memory-mapped files. These interfaces are for traditional **volatile** memory
allocation but, unlike the functions described in **malloc**(3), the memory managed by **libvmem** may have different attributes, depending on the file system
containing the memory-mapped files. In particular, **libvmem** is part of the *Non-Volatile Memory Library* because it is sometimes useful to use non-volatile
memory as a volatile memory pool, leveraging its capacity, cost, or performance characteristics.

**libvmem** uses the **mmap**(2) system call to create a pool of volatile memory. The library is most useful when used with *Direct Access* storage (DAX),
which is memory-addressable persistent storage that supports load/store access without being paged via the system page cache. A Persistent Memory-aware file
system is typically used to provide this type of access. Memory-mapping a file from a Persistent Memory-aware file system provides the raw memory pools, and
this library supplies the more familiar *malloc-like* interfaces on top of those pools.

Under normal usage, **libvmem** will never print messages or intentionally cause the process to exit. Exceptions to this are prints caused by calls to
**vmem_stats_print**(), or by enabling debugging as described under **DEBUGGING AND ERROR HANDLING** below. The library uses **pthreads**(7) to be fully
MT-safe, but never creates or destroys threads itself. The library does not make use of any signals, networking, and never calls **select**() or **poll**().
The system memory allocation routines like **malloc**() and **free**() are used by **libvmem** for managing a small amount of run-time state, but applications
are allowed to override these calls if necessary (see the description of **vmem_set_funcs**() below).

**libvmem** interfaces are grouped into three categories: those that manage memory pools, those providing the basic memory allocation functions, and those
interfaces less commonly used for managing the overall library behavior. These groups of interfaces are described in the following three sections.


# MANAGING MEMORY POOLS #

To use **libvmem**, a *memory pool* is first created. This is most commonly done with the !vmem_create function described in this section. The other
functions described in this section are for less common cases, where applications have special needs for creating pools or examining library state.

Once created, a memory pool is represented by an opaque pool handle, of type *VMEM\**, which is passed to the functions for memory allocation described in the
next section.

```c
!ifdef{WIN32}
{
VMEM *vmem_createU(const char *dir, size_t size);
VMEM *vmem_createW(const wchar_t *dir, size_t size);
}{
VMEM *vmem_create(const char *dir, size_t size);
}
```

The !vmem_create function creates a memory pool. The resulting pool is then used with functions like **vmem_malloc**() and **vmem_free**() to provide the
familiar *malloc-like* programming model for the memory pool. !vmem_create creates the pool by allocating a temporary file in the given directory *dir*.
The file is created in a fashion similar to **tmpfile**(3), so that the file name does not appear when the directory is listed and the space is automatically
freed when the program terminates. *size* bytes are allocated and the resulting space is memory-mapped. The minimum *size* value allowed by the library is
defined in **\<libvmem.h\>** as **VMEM_MIN_POOL**. Calling !vmem_create with a size smaller than that will return an error. The maximum allowed size is not
limited by **libvmem**, but by the file system specified by the *dir* argument. The *size* passed in is the raw size of the memory pool and **libvmem** will
use some of that space for its own metadata. !vmem_create returns an opaque memory pool handle or NULL if an error occurred (in which case *errno* is set
appropriately). The opaque memory pool handle is then used with the other functions described in this man page that operate on a specific memory pool.

This function can also be called with the **dir** argument pointing to a device
DAX, and in that case the entire device will serve as a volatile pool.
Device DAX is the device-centric analogue of Filesystem DAX. It allows memory
ranges to be allocated and mapped without need of an intervening file system.
For more information please see **ndctl-create-namespace**(1).

```c
VMEM *vmem_create_in_region(void *addr, size_t size);
```

The **vmem_create_in_region**() is an alternate **libvmem** entry point for creating a memory pool. It is for the rare case where an application needs to
create a memory pool from an already memory-mapped region. Instead of allocating space from a given file system, **vmem_create_in_region**() is given the
memory region explicitly via the *addr* and *size* arguments. Any data in the region is lost by calling **vmem_create_in_region**(), which will immediately
store its own data structures for managing the pool there. Like !vmem_create above, the minimum *size* allowed is defined as **VMEM_MIN_POOL**. The *addr*
argument must be page aligned. **vmem_create_in_region**() returns an opaque memory pool handle or NULL if an error occurred (in which case *errno* is set
appropriately). Undefined behavior occurs if *addr* does not point to the contiguous memory region in the virtual address space of the calling process, or if
the *size* is larger than the actual size of the memory region pointed by *addr*.

```c
void vmem_delete(VMEM *vmp);
```

The **vmem_delete**() function releases the memory pool *vmp*. If the memory pool was created using !vmem_create, deleting it allows the space to be
reclaimed.

```c
int vmem_check(VMEM *vmp);
```

The **vmem_check**() function performs an extensive consistency check of all **libvmem** internal data structures in memory pool *vmp*. It returns 1 if the
memory pool during the check is found to be consistent and 0 otherwise. Cases where the check couldn't be performed, are indicated by a return value of -1.
Since an error return indicates memory pool corruption, applications should not continue to use a pool in this state. Additional details about errors found are
logged when the log level is at least 1 (see **DEBUGGING AND ERROR HANDLING** below). During the consistency check performed by **vmem_check**(), other
operations on the same memory pool are locked out. The checks are all read-only; **vmem_check**() never modifies the memory pool. This function is mostly
useful for **libvmem** developers during testing/debugging.

```c
void vmem_stats_print(VMEM *vmp, const char *opts);
```

The **vmem_stats_print**() function produces messages containing statistics about the given memory pool. The output is printed using **libvmem**'s internal
*print_func* function (see **vmem_set_funcs**() below). That means the output typically appears on *stderr* unless the caller supplies a replacement
*print_func* or sets the environment variable **VMEM_LOG_FILE** to direct output elsewhere. The *opts* string can either be NULL or it can contain a list of
options that change the stats printed. General information that never changes during execution can be omitted by specifying "g" as a character within the
opts string. The characters "m" and "a" can be specified to omit merged arena and per arena statistics, respectively; "b" and "l" can be
specified to omit per size class statistics for bins and large objects, respectively. Unrecognized characters are silently ignored. Note that thread caching
may prevent some statistics from being completely up to date. See **jemalloc**(3) for more detail (the description of the available *opts* above was taken from
that man page).


# MEMORY ALLOCATION #

This section describes the *malloc-like* API provided by **libvmem**. These functions provide the same semantics as their libc namesakes,
but operate on the memory pools specified by their first arguments.

```c
void *vmem_malloc(VMEM *vmp, size_t size);
```

The **vmem_malloc**() function provides the same semantics as **malloc**(3), but operates on the memory pool *vmp* instead of the process heap supplied by the
system. It allocates *size* bytes and returns a pointer to the allocated memory. *The memory is not initialized*. If *size* is 0, then **vmem_malloc**()
returns either NULL, or a unique pointer value that can later be successfully passed to **vmem_free**(). If **vmem_malloc**() is unable to satisfy the
allocation request, a NULL pointer is returned and *errno* is set appropriately.

```c
void vmem_free(VMEM *vmp, void *ptr);
```

The **vmem_free**() function provides the same semantics as **free**(3), but operates on the memory pool *vmp* instead of the process heap supplied by the
system. It frees the memory space pointed to by *ptr*, which must have been returned by a previous call to **vmem_malloc**(), **vmem_calloc**() or
**vmem_realloc**() for *the same pool of memory*. Undefined behavior occurs if frees do not correspond to allocated memory from the same memory pool. If *ptr*
is NULL, no operation is performed.

```c
void *vmem_calloc(VMEM *vmp, size_t nmemb, size_t size);
```

The **vmem_calloc**() function provides the same semantics as **calloc**(3), but operates on the memory pool *vmp* instead of the process heap supplied by the
system. It allocates memory for an array of *nmemb* elements of *size* bytes each and returns a pointer to the allocated memory. The memory is set to zero. If
*nmemb* or *size* is 0, then **vmem_calloc**() returns either NULL, or a unique pointer value that can later be successfully passed to **vmem_free**(). If
**vmem_calloc**() is unable to satisfy the allocation request, a NULL pointer is returned and *errno* is set appropriately.

```c
void *vmem_realloc(VMEM *vmp, void *ptr, size_t size);
```

The **vmem_realloc**() function provides the same semantics as **realloc**(3), but operates on the memory pool *vmp* instead of the process heap supplied by
the system. It changes the size of the memory block pointed to by *ptr* to *size* bytes. The contents will be unchanged in the range from the start of the
region up to the minimum of the old and new sizes. If the new size is larger than the old size, the added memory will *not* be initialized. If *ptr* is NULL,
then the call is equivalent to *vmem_malloc(vmp, size)*, for all values of *size*; if *size* is equal to zero, and *ptr* is not NULL, then the call is
equivalent to *vmem_free(vmp, ptr)*. Unless *ptr* is NULL, it must have been returned by an earlier call to **vmem_malloc**(), **vmem_calloc**() or
**vmem_realloc**(). If the area pointed to was moved, a *vmem_free(vmp, ptr)* is done. If **vmem_realloc**() is unable to satisfy the allocation request, a
NULL pointer is returned and *errno* is set appropriately.

```c
void *vmem_aligned_alloc(VMEM *vmp, size_t alignment, size_t size);
```

The **vmem_aligned_alloc**() function provides the same semantics as **aligned_alloc**(3), but operates on the memory pool *vmp* instead of the process heap
supplied by the system. It allocates *size* bytes from the memory pool and returns a pointer to the allocated memory. The memory address will be a multiple of
*alignment*, which must be a power of two. If **vmem_aligned_alloc**() is unable to satisfy the allocation request, a NULL pointer is returned and *errno* is
set appropriately.

```c
char *vmem_strdup(VMEM *vmp, const char *s);
```

The **vmem_strdup**() function provides the same semantics as **strdup**(3), but operates on the memory pool *vmp* instead of the process heap supplied by the
system. It returns a pointer to a new string which is a duplicate of the string *s*. Memory for the new string is obtained with **vmem_malloc**(), on the given
memory pool, and can be freed with **vmem_free**() on the same memory pool. If **vmem_strdup**() is unable to satisfy the allocation request, a NULL pointer is
returned and *errno* is set appropriately.

```c
wchar_t *vmem_wcsdup(VMEM *vmp, const wchar_t *s);
```

The **vmem_wcsdup**() function provides the same semantics as **wcsdup**(3), but operates on the memory pool *vmp* instead of the process heap supplied by the
system. It returns a pointer to a new wide character string which is a duplicate of the wide character strin string *s*. Memory for the new string is obtained with **vmem_malloc**(), on the given
memory pool, and can be freed with **vmem_free**() on the same memory pool. If **vmem_wcsdup**() is unable to satisfy the allocation request, a NULL pointer is
returned and *errno* is set appropriately.

```c
size_t vmem_malloc_usable_size(VMEM *vmp, void *ptr);
```

The **vmem_malloc_usable_size**() function provides the same semantics as **malloc_usable_size**(3), but operates on the memory pool *vmp* instead of the
process heap supplied by the system. It returns the number of usable bytes in the block of allocated memory pointed to by *ptr*, a pointer to a block of memory
allocated by **vmem_malloc**() or a related function. If *ptr* is NULL, 0 is returned.


# MANAGING LIBRARY BEHAVIOR #

The library entry points described in this section are less commonly used than the previous section. These entry points expose library information or alter the
default library behavior.


```c
!ifdef{WIN32}
{
const char *vmem_check_versionU(
	unsigned major_required,
	unsigned minor_required);
const wchar_t *vmem_check_versionW(
	unsigned major_required,
	unsigned minor_required);
}{
const char *vmem_check_version(
	unsigned major_required,
	unsigned minor_required);
}
```

The !vmem_check_version function is used to see if the installed **libvmem** supports the version of the library API required by an application. The
easiest way to do this is for the application to supply the compile-time version information, supplied by defines in **\<libvmem.h\>**, like this:

```c
reason = vmem_check_version!U{}(VMEM_MAJOR_VERSION,
                            VMEM_MINOR_VERSION);
if (reason != NULL) {
	/* version check failed, reason string tells you why */
}
```

Any mismatch in the major version number is considered a failure, but a library with a newer minor version number will pass this check since increasing minor
versions imply backwards compatibility.

An application can also check specifically for the existence of an interface by checking for the version where that interface was introduced. These versions
are documented in this man page as follows: unless otherwise specified, all interfaces described here are available in version 1.0 of the library. Interfaces
added after version 1.0 will contain the text *introduced in version x.y* in the section of this manual describing the feature.

When the version check performed by !vmem_check_version is successful, the return value is NULL. Otherwise the return value is a static string describing
the reason for failing the version check. The string returned by !vmem_check_version must not be modified or freed.

```c
void vmem_set_funcs(
	void *(*malloc_func)(size_t size),
	void (*free_func)(void *ptr),
	void *(*realloc_func)(void *ptr, size_t size),
	char *(*strdup_func)(const char *s),
	void (*print_func)(const char *s));
```

The **vmem_set_funcs**() function allows an application to override some interfaces used internally by **libvmem**. Passing in NULL for any of the handlers
will cause the **libvmem** default function to be used. The library does not make heavy use of the system malloc functions, but it does allocate approximately
4-8 kilobytes for each memory pool in use. The only functions in the malloc family used by the library are represented by the first four arguments to
**vmem_set_funcs**(). The *print_func* function is called by **libvmem** when the **vmem_stats_print**() entry point is used, or when additional tracing is
enabled in the debug version of the library as described in the **DEBUGGING AND ERROR HANDLING** section below. The default *print_func* used by the library
prints to the file specified by the **VMEM_LOG_FILE** environment variable, or to *stderr* if that variable is not set.


# CAVEATS #

**libvmem** relies on the library destructor being called from the main thread.
For this reason, all functions that might trigger destruction (e.g.
**dlclose**()) should be called in the main thread. Otherwise some of the
resources associated with that thread might not be cleaned up properly.


# DEBUGGING AND ERROR HANDLING #

Two versions of **libvmem** are typically available on a development system. The normal version, accessed when a program is linked using the **-lvmem** option,
is optimized for performance. That version skips checks that impact performance and never logs any trace information or performs any run-time assertions. If an
error is detected during the call to **libvmem** function, an application may retrieve an error message describing the reason of failure using the following
function:

```c
!ifdef{WIN32}
{
const char *vmem_errormsgU(void);
const wchar_t *vmem_errormsgW(void);
}{
const char *vmem_errormsg(void);
}
```

The !vmem_errormsg function returns a pointer to a static buffer containing the last error message logged for current thread. The error message may
include description of the corresponding error code (if *errno* was set), as returned by **strerror**(3). The error message buffer is thread-local; errors
encountered in one thread do not affect its value in other threads. The buffer is never cleared by any library function; its content is significant only when
the return value of the immediately preceding call to **libvmem** function indicated an error, or if *errno* was set. The application must not modify or free
the error message string, but it may be modified by subsequent calls to other library functions.

A second version of **libvmem**, accessed when a program uses
the libraries under !ifdef{WIN32}{**/nvml/src/x64/Debug**}{**/usr/lib/nvml_debug**}, contains
run-time assertions and trace points. The typical way to
access the debug version is to set the environment variable
**LD_LIBRARY_PATH** to !ifdef{WIN32}{**/nvml/src/x64/Debug** or other location}
{**/usr/lib/nvml_debug** or **/usr/lib64/nvml_debug**} depending on where the debug
libraries are installed on the system.
The trace points in the debug version of the library are enabled using the environment variable
**VMEM_LOG_LEVEL**, which can be set to the following values:

+ **0** - This is the default level when **VMEM_LOG_LEVEL** is not set. Only statistics are logged, and then only in response to a call to **vmem_stats_print**().

+ **1** - Additional details on any errors detected are logged (in addition to returning the *errno*-based errors as usual). The same information may be
retrieved using !vmem_errormsg.

+ **2** - A trace of basic operations including allocations and deallocations is logged.

+ **3** - This level enables a very verbose amount of function call tracing in the library.

+ **4** - This level enables voluminous and fairly obscure tracing information that is likely only useful to the **libvmem** developers.

The environment variable **VMEM_LOG_FILE** specifies a file name where all logging information should be written. If the last character in the name is "-", the
PID of the current process will be appended to the file name when the log file is created. If **VMEM_LOG_FILE** is not set, output goes to stderr. All prints are
done using the *print_func* function in **libvmem** (see **vmem_set_funcs**() above for details on how to override that function).

Setting the environment variable **VMEM_LOG_LEVEL** has no effect on the non-debug version of **libvmem**.


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
	if ((vmp = vmem_create!U{}("/pmem-fs",
			VMEM_MIN_POOL)) == NULL) {
		perror("vmem_create!U");
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

**malloc**(3), **posix_memalign**(3), **strdup**(3), **mmap**(2),
**strerror**(3), **jemalloc**(3), **libpmem**(3), **ndctl-create-namespace**(1)
and **<http://pmem.io>**

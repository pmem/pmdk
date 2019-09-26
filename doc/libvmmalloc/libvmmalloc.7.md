---
layout: manual
Content-Style: 'text/css'
title: _MP(LIBVMMALLOC, 7)
collection: libvmmalloc
header: PMDK
date: vmmalloc API version 1.1
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

[comment]: <> (libvmmalloc.7 -- man page for libvmmalloc)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[ENVIRONMENT](#environment)<br />
[CAVEATS](#caveats)<br />
[DEBUGGING](#debugging)<br />
[NOTES](#notes)<br />
[BUGS](#bugs)<br />
[ACKNOWLEDGEMENTS](#acknowledgements)<br />
[SEE ALSO](#see-also)


# NAME #

**libvmmalloc** - general purpose volatile memory allocation library


# SYNOPSIS #

```
$ LD_PRELOAD=libvmmalloc.so.1 command [ args... ]
```

or

```c
#include <stdlib.h>
#ifndef __FreeBSD__
    #include <malloc.h>
#else
    #include <malloc_np.h>
#endif
#include <libvmmalloc.h>

cc [ flag... ] file... -lvmmalloc [ library... ]
```

```c
void *malloc(size_t size);
void free(void *ptr);
void *calloc(size_t number, size_t size);
void *realloc(void *ptr, size_t size);
int posix_memalign(void **memptr, size_t alignment, size_t size);
void *aligned_alloc(size_t alignment, size_t size);
void *memalign(size_t alignment, size_t size);
void *valloc(size_t size);
void *pvalloc(size_t size);
size_t malloc_usable_size(const void *ptr);
void cfree(void *ptr);
```


# DESCRIPTION #

**libvmmalloc** transparently converts all dynamic memory allocations
into Persistent Memory allocations.

The typical usage of **libvmmalloc** does not require any modification of
the target program. It is enough to load **libvmmalloc** before all other
libraries by setting the environment variable **LD_PRELOAD**. When used in
that way, **libvmmalloc** interposes the standard system memory allocation
routines, as defined in **malloc**(3), **posix_memalign**(3) and
**malloc_usable_size**(3), and provides that all dynamic memory allocations
are made from a *memory pool* built on a memory-mapped file, instead of the
system heap. The memory managed by **libvmmalloc** may have different
attributes, depending on the file system containing the memory-mapped file.
In particular, **libvmmalloc** is part of the *Persistent Memory Development Kit*
because it is sometimes useful to use non-volatile memory as a volatile
memory pool, leveraging its capacity, cost, or performance characteristics.

This library is no longer actively developed, and is in maintenance mode,
same as its underlying code backend (**libvmem**).  It is mature, and is
expected to be supported for foreseable future.

**libvmmalloc** may be also linked to the program, by providing the
**-lvmmalloc* argument to the linker. Then it becomes the default memory
allocator for the program.

>NOTE:
Due to the fact the library operates on a memory-mapped file, **it may not
work properly with programs that perform fork(2) not followed by exec(3).**
There are two variants of experimental **fork**(2) support available in
libvmmalloc. The desired library behavior may be selected by setting the
**VMMALLOC_FORK** environment variable. By default variant #1 is enabled.
See **ENVIRONMENT** for more details.

**libvmmalloc** uses the **mmap**(2) system call to create a pool of
volatile memory. The library is most useful when used with *Direct Access*
storage (DAX), which is memory-addressable persistent storage that supports
load/store access without being paged via the system page cache. A Persistent
Memory-aware file system is typically used to provide this type of access.
Memory-mapping a file from a Persistent Memory-aware file system provides the
raw memory pools, and this library supplies the traditional
*malloc* interfaces on top of those pools.

The memory pool acting as a system heap replacement is created automatically
at library initialization time. The user may control its location and size by
setting the environment variables described in **ENVIRONMENT**, below. The
allocated file space is reclaimed when the process terminates or in case of
system crash.

Under normal usage, **libvmmalloc** will never print messages or intentionally
cause the process to exit. The library uses **pthreads**(7) to be fully
MT-safe, but never creates or destroys threads itself. The library does not
make use of any signals, networking, and never calls **select**(2) or
**poll**(2).


# ENVIRONMENT #

The **VMMALLOC_POOL_DIR** and **VMMALLOC_POOL_SIZE** environment variables
**must** be set for **libvmmalloc** to work properly. If either of them is
not specified, or if their values are not valid, the library prints an
appropriate error message and terminates the process. Any other environment
variables are optional.

+ **VMMALLOC_POOL_DIR**=*path*

Specifies a path to the directory where the memory pool file should be created.
The directory must exist and be writable.

+ **VMMALLOC_POOL_SIZE**=*len*

Defines the desired size (in bytes) of the memory pool file. It must be not
less than the minimum allowed size **VMMALLOC_MIN_POOL** as defined in
**\<libvmmalloc.h\>**.

>NOTE:
Due to the fact the library adds some metadata to the memory pool, the amount of
actual usable space is typically less than the size of the memory pool file.

+ **VMMALLOC_FORK**=*val* (EXPERIMENTAL)

**VMMALLOC_FORK** controls the behavior of **libvmmalloc** in case of
**fork**(3), and can be set to the following values:

+ **0** - **fork**(2) support is disabled. The behavior of **fork**(2) is
undefined in this case, but most likely results in memory pool corruption
and a program crash due to segmentation fault.

+ **1** - The memory pool file is remapped with the **MAP_PRIVATE** flag before
the fork completes. From this moment, any access to memory that modifies the
heap pages, both in the parent and in the child process, will trigger creation
of a copy of those pages in RAM (copy-on-write). The benefit of this approach
is that it does not significantly increase the time of the initial fork
operation, and does not require additional space on the file system. However,
all subsequent memory allocations, and modifications of any memory allocated
before fork, will consume system memory resources instead of the memory pool.

This is the default option if **VMMALLOC_FORK** is not set.

+ **2** - A copy of the entire memory pool file is created for the use of the
child process. This requires additional space on the file system, but both the
parent and the child process may still operate on their memory pools, not
consuming system memory resources.

>NOTE:
In case of large memory pools, creating a copy of the pool file may stall
the fork operation for a quite long time.

+ **3** - The library first attempts to create a copy of the memory pool
(as for option #2), but if it fails (i.e. because of insufficient free
space on the file system), it will fall back to option #1.

>NOTE: Options **2** and **3** are not currently supported on FreeBSD.

Environment variables used for debugging are described in **DEBUGGING**,
below.

# CAVEATS #

**libvmmalloc** relies on the library destructor being called from the main
thread. For this reason, all functions that might trigger destruction (e.g.
**dlclose**(3)) should be called in the main thread. Otherwise some of the
resources associated with that thread might not be cleaned up properly.


# DEBUGGING #

Two versions of **libvmmalloc** are typically available on a development
system. The normal version is optimized for performance. That version skips
checks that impact performance and never logs any trace information or
performs any run-time assertions. A second version, accessed when using
libraries from _DEBUGLIBPATH(), contains run-time assertions and trace
points. The typical way to access the debug version is to set the
**LD_LIBRARY_PATH** environment variable to _LDLIBPATH(). Debugging output is
controlled using the following environment variables. These variables have
no effect on the non-debug version of the library.

+ **VMMALLOC_LOG_LEVEL**

The value of **VMMALLOC_LOG_LEVEL** enables trace points in the debug version
of the library, as follows:

+ **0** - Tracing is disabled. This is the default level when
**VMMALLOC_LOG_LEVEL** is not set.

+ **1** - Additional details on any errors detected are logged, in addition to
returning the *errno*-based errors as usual.

+ **2** - A trace of basic operations is logged.

+ **3** - Enables a very verbose amount of function call tracing
in the library.

+ **4** - Enables voluminous tracing information about all memory
allocations and deallocations.

Unless **VMMALLOC_LOG_FILE** is set, debugging output is written to *stderr*.

+ **VMMALLOC_LOG_FILE**

Specifies the name of a file where
all logging information should be written. If the last character in the name
is "-", the *PID* of the current process will be appended to the file name when
the log file is created. If **VMMALLOC_LOG_FILE** is not set, output is
written to *stderr*.

+ **VMMALLOC_LOG_STATS**

Setting **VMMALLOC_LOG_STATS** to 1 enables logging human-readable
summary statistics at program termination.


# NOTES #

Unlike the normal **malloc**(3), which asks the system for additional memory
when it runs out, **libvmmalloc** allocates the size it is told to and never
attempts to grow or shrink that memory pool.


# BUGS #

**libvmmalloc** may not work properly with programs that perform **fork**(2)
and do not call **exec**(3) immediately afterwards. See **ENVIRONMENT**
for more details about experimental **fork**(2) support.

If logging is enabled in the debug version of the library and the process
performs **fork**(2), no new log file is created for the child process, even if
the configured log file name ends with "-". All logging information from
the child process will be written to the log file owned by the parent process,
which may lead to corruption or partial loss of log data.

Malloc hooks (see **malloc_hook**(3)), are not supported when using
**libvmmalloc**.


# ACKNOWLEDGEMENTS #

**libvmmalloc** depends on jemalloc, written by Jason Evans, to do the heavy
lifting of managing dynamic memory allocation. See:
<http://www.canonware.com/jemalloc>


# SEE ALSO #

**fork**(2), **dlclose(3)**, **exec**(3), **malloc**(3),
**malloc_usable_size**(3), **posix_memalign**(3),
**libpmem**(7), **libvmem**(7) and **<http://pmem.io>**

On Linux:

**jemalloc**(3), **malloc_hook**(3), **pthreads**(7), **ld.so**(8)

On FreeBSD:

**ld.so**(1), **pthread**(3)

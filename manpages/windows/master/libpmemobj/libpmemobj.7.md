---
layout: manual
Content-Style: 'text/css'
title: LIBPMEMOBJ
collection: libpmemobj
header: NVM Library
date: pmemobj API version 2.2
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

[comment]: <> (libpmemobj.7 -- man page for libpmemobj)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />


[TRANSACTIONAL OBJECT MANIPULATION](#transactional-object-manipulation-1)<br />
[CAVEATS](#caveats)<br />
[LIBRARY API VERSIONING](#library-api-versioning-1)<br />
[MANAGING LIBRARY BEHAVIOR](#managing-library-behavior)<br />
[DEBUGGING AND ERROR HANDLING](#debugging-and-error-handling)<br />
[EXAMPLE](#example)<br />
[ACKNOWLEDGEMENTS](#acknowledgements)<br />
[SEE ALSO](#see-also)<br />


# NAME #

**libpmemobj** -- persistent memory transactional object store


# SYNOPSIS #

```c
#include <libpmemobj.h>
cc -std=gnu99 ... -lpmemobj -lpmem
```

>NOTE: NVML API supports UNICODE. If **NVML_UTF8_API** macro is defined then
basic API functions are expanded to UTF-8 API with postfix *U*,
otherwise they are expanded to UNICODE API with postfix *W*.

##### Library API versioning: #####

```c
const char *pmemobj_check_versionU(
	unsigned major_required,
	unsigned minor_required);
const wchar_t *pmemobj_check_versionW(
	unsigned major_required,
	unsigned minor_required);
```

##### Managing library behavior: #####

```c
void pmemobj_set_funcs(
	void *(*malloc_func)(size_t size),
	void (*free_func)(void *ptr),
	void *(*realloc_func)(void *ptr, size_t size),
	char *(*strdup_func)(const char *s));
```

##### Error handling: #####

```c
const char *pmemobj_errormsgU(void);
const wchar_t *pmemobj_errormsgW(void);
```

##### Other library functions: #####

A description of other **libpmemlog** functions can be found on different manual pages:
* most commonly used functions: **pmemobj_open**(3)
* low-level memory manipulation: **pmemobj_memcpy_persist**(3)
* locking: **pmemobj_mutex_zero**(3)
* persistent object identifier: **OID_IS_NULL**(3)
* type-safety: **TOID_DECLARE**(3)
* layout declaration: **POBJ_LAYOUT_BEGIN**(3)
* non-transactional atomic allocations: **pmemobj_alloc**(3)
* root object management: **pmemobj_root**(3)
* object containers: **pmemobj_first**(3)
* non-transactional persistent atomic circular doubly-linked list:
**pmemobj_list_insert**(3), **POBJ_LIST_HEAD**(3)
* transactional object manipulation: **pmemobj_tx_begin**(3),
**pmemobj_tx_add_range**(3), **pmemobj_tx_alloc**(3)
* control and statistics: **pmemobj_ctl_get**(3)


# DESCRIPTION #

**libpmemobj** provides a transactional object store in *persistent memory* (pmem).
This library is intended for applications using direct access storage
(DAX), which is storage that supports load/store access without paging blocks
from a block storage device. Some types of *non-volatile memory DIMMs* (NVDIMMs)
provide this type of byte addressable access to storage. A *persistent memory aware
file system* is typically used to expose the direct access to applications.
Memory mapping a file from this type of file system results in the load/store,
non-paged access to pmem. **libpmemobj** builds on this type of memory mapped file.

This library is for applications that need a transactions and persistent memory management.

This library builds on the low-level pmem support provided by **libpmem**, handling
the transactional updates, flushing changes to persistence, and recovery for the application.

**libpmemobj** is one of a collection of persistent memory libraries available, the others are:

+ **libpmemblk**(3), providing pmem-resident arrays of fixed-sized blocks with atomic updates.

+ **libpmemlog**(3), providing a pmem-resident log file.

+ **libpmem**(3), low-level persistent memory support.

Under normal usage, **libpmemobj** will never print messages or intentionally cause
the process to exit. The only exception to this is the debugging
information, when enabled, as described under **DEBUGGING AND ERROR HANDLING** below.


# LIBRARY API VERSIONING #

This section describes how the library API is versioned,
allowing applications to work with an evolving API.

The **pmemobj_check_versionU**()/**pmemobj_check_versionW**() function is used to see if the installed **libpmemobj**
supports the version of the library API required by an application. The easiest way
to do this is for the application to supply the compile-time version information,
supplied by defines in **\<libpmemobj.h\>**, like this:

```c
reason = pmemobj_check_versionU(PMEMOBJ_MAJOR_VERSION,
                               PMEMOBJ_MINOR_VERSION);
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
added after version 1.0 will contain the text *introduced in version x.y* in
the section of this manual describing the feature.

When the version check performed by **pmemobj_check_versionU**()/**pmemobj_check_versionW**() is successful, the retur
value is NULL. Otherwise the return value is a static string describing the reason
for failing the version check. The string returned by **pmemobj_check_versionU**()/**pmemobj_check_versionW**()
must not be modified or freed.


# MANAGING LIBRARY BEHAVIOR #

The library entry points described in this section
are less commonly used than the previous sections.

The **pmemobj_set_funcs**() function allows an application to override memory
allocation calls used internally by **libpmemobj**. Passing in NULL for any of
the handlers will cause the **libpmemobj** default function to be used. The library
does not make heavy use of the system malloc functions, but it does allocate
approximately 4-8 kilobytes for each memory pool in use.


# DEBUGGING AND ERROR HANDLING #


If an error is detected during the call to **libpmemobj** function, an application may
retrieve an error message describing the reason of failure.

The **pmemobj_errormsgU**()/**pmemobj_errormsgW**() function returns a pointer to a static buffer containing
the last error message logged for current thread. The error message may
include description of the corresponding error code (if *errno* was set),
as returned by **strerror**(3). The error message buffer is thread-local;
errors encountered in one thread do not affect its value in other threads.
The buffer is never cleared by any library function; its content is significant
only when the return value of the immediately preceding call to **libpmemobj**
function indicated an error, or if *errno* was set. The application must not modify
or free the error message string, but it may be modified by subsequent calls to other
library functions.

A second version of **libpmemobj**, accessed when a program uses
the libraries under **/nvml/src/x64/Debug**,
contains run-time assertions and trace points. The typical way to
access the debug version is to set the environment variable
**LD_LIBRARY_PATH** to **/nvml/src/x64/Debug** or other location depending on where the debug
libraries are installed on the system.
The trace points in the debug version of the library are enabled using the environment
variable **PMEMOBJ_LOG_LEVEL** which can be set to the following values:

+ **0** - This is the default level when **PMEMOBJ_LOG_LEVEL** is not set.
No log messages are emitted at this level.

+ **1** - Additional details on any errors detected are logged
(in addition to returning the *errno*-based errors as usual).
The same information may be retrieved using **pmemobj_errormsgU**()/**pmemobj_errormsgW**().

+ **2** - A trace of basic operations is logged.

+ **3** - This level enables a very verbose amount of function call
tracing in the library.

+ **4** - This level enables voluminous and fairly obscure tracing information
that is likely only useful to the **libpmemobj** developers.

The environment variable **PMEMOBJ_LOG_FILE** specifies a file name where all
logging information should be written. If the last character in the name is
"-", the PID of the current process will be appended to the file name when the
log file is created. If **PMEMOBJ_LOG_FILE** is not set, the logging output
goes to stderr.

Setting the environment variable **PMEMOBJ_LOG_LEVEL** has no effect on the
non-debug version of **libpmemobj**. See also **libpmem**(7) to get information
about other environment variables affecting **libpmemobj** behavior.

**libpmemobj** by default supports up to 1024 parallel transactions / allocations.
For debugging purposes it is possible to decrease this value by writing
a desired limit to the **PMEMOBJ_NLANES** environment variable.


# EXAMPLE #

See <http://pmem.io/nvml/libpmemobj> for examples using the **libpmemobj** API.


# ACKNOWLEDGEMENTS #

**libpmemobj** builds on the persistent memory programming model recommended
by the SNIA NVM Programming Technical Work Group:
<http://snia.org/nvmp>


# SEE ALSO #

**OID_IS_NULL**(3), **pmemobj_alloc**(3), **pmemobj_ctl_get**(3),
**pmemobj_ctl_set**(3), **pmemobj_first**(3), **pmemobj_list_insert**(3),
**pmemobj_memcpy_persist**(3), **pmemobj_mutex_zero**(3), **pmemobj_open**(3),
**pmemobj_root**(3), **pmemobj_tx_add_range**(3), **pmemobj_tx_alloc**(3),
**pmemobj_tx_begin**(3), **POBJ_LAYOUT_BEGIN**(3), **POBJ_LIST_HEAD**(3),
**strerror**(3), **TOID_DECLARE**(3), **libpmemblk**(7), **libpmemlog**(7),
**libpmem**(7), **libvmem**(7) and **<http://pmem.io>**

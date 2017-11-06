---
layout: manual
Content-Style: 'text/css'
title: LIBPMEM!7
collection: libpmem
header: NVM Library
date: pmem API version 1.0
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

[comment]: <> (libpmem.7 -- man page for libpmem)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[LIBRARY API VERSIONING](#library-api-versioning-1)<br />
[DEBUGGING AND ERROR HANDLING](#debugging-and-error-handling)<br />
[ENVIRONMENT VARIABLES](#environment-variables)<br />
[EXAMPLE](#example)<br />
[ACKNOWLEDGEMENTS](#acknowledgements)<br />
[SEE ALSO](#see-also)


# NAME #

**libpmem** -- persistent memory support library


# SYNOPSIS #

```c
#include <libpmem.h>
cc ... -lpmem
```

!ifdef{WIN32}
{
>NOTE: NVML API supports UNICODE. If **NVML_UTF8_API** macro is defined then
basic API functions are expanded to UTF-8 API with postfix *U*,
otherwise they are expanded to UNICODE API with postfix *W*.
}

##### Library API versioning: #####

```c
!ifdef{WIN32}
{
const char *pmem_check_versionU(
	unsigned major_required,
	unsigned minor_required);
const wchar_t *pmem_check_versionW(
	unsigned major_required,
	unsigned minor_required);
}{
const char *pmem_check_version(
	unsigned major_required,
	unsigned minor_required);
}
```

##### Error handling: #####

```c
!ifdef{WIN32}
{
const char *pmem_errormsgU(void);
const wchar_t *pmem_errormsgW(void);
}{
const char *pmem_errormsg(void);
}
```

##### Other library functions: #####

A description of other **libpmem** functions can be found on different manual pages:
* most commonly used functions: **pmem_is_pmem**(3)
* partial flushing operations: **pmem_flush**(3)
* copying to persistent memory: **pmem_memmove_persist**(3)


# DESCRIPTION #

**libpmem** provides low-level *persistent memory* (pmem) support for
applications using direct access storage (DAX), which is storage that
supports load/store access without paging blocks from a block storage
device. Some types of *non-volatile memory DIMMs* (NVDIMMs) provide this
type of byte addressable access to storage. A *persistent memory aware
file system* is typically used to expose the direct access to
applications. Memory mapping a file from this type of file system
results in the load/store, non-paged access to pmem.

This library is for applications that use persistent memory directly,
without the help of any library-supplied transactions or memory
allocation. Higher-level libraries that build on **libpmem** are
available and are recommended for most applications, see:

+ **libpmemobj**(7), a general use persistent memory API, providing memory
  allocation and transactional operations on variable-sized objects.

+ **libpmemblk**(7), providing pmem-resident arrays of fixed-sized blocks
  with atomic updates.

+ **libpmemlog**(7), providing a pmem-resident log file.

Under normal usage, **libpmem** will never print messages or
intentionally cause the process to exit.
The only exception to this is the debugging information, when enabled,
as described under **DEBUGGING AND ERROR HANDLING** below.


# CAVEATS #

**libpmem** relies on the library destructor being called from the main thread.
For this reason, all functions that might trigger destruction (e.g.
**dlclose**()) should be called in the main thread. Otherwise some of the
resources associated with that thread might not be cleaned up properly.


# LIBRARY API VERSIONING #

This section describes how the library API is versioned, allowing
applications to work with an evolving API.

The !pmem_check_version function is used to see if the installed
**libpmem** supports the version of the library API required by an
application. The easiest way to do this is for the application to supply
the compile-time version information, supplied by defines in
**\<libpmem.h\>**, like this:

```c
reason = pmem_check_version!U{}(PMEM_MAJOR_VERSION,
                            PMEM_MINOR_VERSION);
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

When the version check performed by !pmem_check_version is
successful, the return value is NULL. Otherwise the return value is a
static string describing the reason for failing the version check. The
string returned by !pmem_check_version must not be modified or
freed.


# DEBUGGING AND ERROR HANDLING #

Two versions of **libpmem** are typically available on a development
system. The normal version, accessed when a program is linked using the
**-lpmem** option, is optimized for performance. That version skips
checks that impact performance and never logs any trace information or
performs any run-time assertions. If an error is detected during the
call to **libpmem** function, an application may retrieve an error
message describing the reason of failure using the !pmem_errormsg function.

The !pmem_errormsg function returns a pointer to a static buffer
containing the last error message logged for current thread. The error
message may include description of the corresponding error code (if
*errno* was set), as returned by **strerror**(3). The error message buffer
is thread-local; errors encountered in one thread do not affect its
value in other threads. The buffer is never cleared by any library
function; its content is significant only when the return value of the
immediately preceding call to **libpmem** function indicated an error,
or if *errno* was set. The application must not modify or free the error
message string, but it may be modified by subsequent calls to other
library functions.

A second version of **libpmem**, accessed when a program uses
the libraries under !ifdef{WIN32}{**/nvml/src/x64/Debug**}{**/usr/lib/nvml_debug**}, contains
run-time assertions and trace points. The typical way to
access the debug version is to set the environment variable
**LD_LIBRARY_PATH** to !ifdef{WIN32}{**/nvml/src/x64/Debug** or other location}
{**/usr/lib/nvml_debug** or **/usr/lib64/nvml_debug**} depending on where the debug
libraries are installed on the system. The trace points
in the debug version of the library are enabled using the environment
variable **PMEM_LOG_LEVEL**, which can be set to the following values:

+ **0** - This is the default level when **PMEM_LOG_LEVEL** is not set.
  No log messages are emitted at this level.

+ **1** - Additional details on any errors detected are logged (in addition
  to returning the *errno*-based errors as usual). The same information
  may be retrieved using !pmem_errormsg.

+ **2** - A trace of basic operations is logged.

+ **3** - This level enables a very verbose amount of function call
  tracing in the library.

+ **4** - This level enables voluminous and fairly obscure tracing
  information that is likely only useful to the **libpmem** developers.

The environment variable **PMEM_LOG_FILE** specifies a file name where
all logging information should be written. If the last character in the
name is "-", the PID of the current process will be appended to the file
name when the log file is created. If **PMEM_LOG_FILE** is not set,
the logging output goes to stderr.

Setting the environment variable **PMEM_LOG_LEVEL** has no effect on
the non-debug version of **libpmem**.


# ENVIRONMENT VARIABLES #

**libpmem** can change its default behavior based on the following
environment variables. These are largely intended for testing and are
not normally required.

+ **PMEM_IS_PMEM_FORCE**=*val*

If *val* is 0 (zero), then **pmem_is_pmem**(4) will always return
false. Setting *val* to 1 causes **pmem_is_pmem**(4) to always return
true. This variable is mostly used for testing but can be used to force
pmem behavior on a system where a range of pmem is not detectable as
pmem for some reason.

>NOTE:
Unlike the other variables, the value of
**PMEM_IS_PMEM_FORCE** is not queried (and cached) at the
library initialization time, but on the first call to
**pmem_is_pmem**(3) function. It means that in case of
**libpmemlog**(7), **libpmemblk**(7), and **libpmemobj**(7)
libraries, it may still be set or modified by the program
until the first attempt to create or open the persistent
memory pool.

+ **PMEM_NO_CLWB**=1

Setting this environment variable to 1 forces **libpmem** to never issue
the **CLWB** instruction on Intel hardware, falling back to other cache
flush instructions instead (**CLFLUSHOPT** or **CLFLUSH** on Intel
hardware). Without this environment variable, **libpmem** will always
use the **CLWB** instruction for flushing processor caches on platforms
that support the instruction. This variable is intended for use during
library testing but may be required for some rare cases where using
**CLWB** has a negative impact on performance.

+ **PMEM_NO_CLFLUSHOPT**=1

Setting this environment variable to 1 forces **libpmem** to never issue
the **CLFLUSHOPT** instruction on Intel hardware, falling back to the
**CLFLUSH** instructions instead. Without this environment variable,
**libpmem** will always use the **CLFLUSHOPT** instruction for flushing
processor caches on platforms that support the instruction, but where
**CLWB** is not available. This variable is intended for use during
library testing.

+ **PMEM_NO_FLUSH**=1

Setting this environment variable to 1 forces **libpmem** to never issue
any of **CLFLUSH**, **CLFLUSHOPT** or **CLWB** instructions on Intel hardware.
This variable is intended for use during library testing.

+ **PMEM_NO_MOVNT**=1

Setting this environment variable to 1 forces **libpmem** to never use
the *non-temporal* move instructions on Intel hardware. Without this
environment variable, **libpmem** will use the non-temporal instructions
for copying larger ranges to persistent memory on platforms that support
the instructions. This variable is intended for use during library
testing.

+ **PMEM_MOVNT_THRESHOLD**=*val*

This environment variable allows overriding the minimal length of
**pmem_memcpy\_\***(), **pmem_memmove\_\***() or
**pmem_memset\_\***() operations, for which **libpmem** uses
*non-temporal* move instructions. Setting this environment variable to 0
forces **libpmem** to always use the *non-temporal* move instructions if
available. It has no effect if **PMEM_NO_MOVNT** variable is set to 1.
This variable is intended for use during library testing.

+ **PMEM_MMAP_HINT**=*val*

This environment variable allows overriding
the hint address used by !pmem_map_file. If set, it also disables
mapping address randomization. This variable is intended for use during
library testing and debugging. Setting it to some fairly large value
(i.e. 0x10000000000) will very likely result in mapping the file at the
specified address (if not used) or at the first unused region above
given address, without adding any random offset. When debugging, this
makes it easier to calculate the actual address of the persistent memory
block, based on its offset in the file. In case of **libpmemobj** it
simplifies conversion of a persistent object identifier (OID) into a
direct pointer to the object.

>NOTE:
**Setting this environment variable
affects all the NVM libraries,** disabling mapping address randomization
and causing the specified address to be used as a hint about where to
place the mapping.


# EXAMPLE #

The following example uses **libpmem** to flush changes made to raw,
memory-mapped persistent memory.

>WARNING:
There is nothing transactional about the **pmem_persist**(3) or
**pmem_msync**(3) calls in this example. Interrupting the program may
result in a partial write to pmem. Use a transactional library such as
**libpmemobj**(7) to avoid torn updates.

```c
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <libpmem.h>

/* using 4k of pmem for this example */
#define PMEM_LEN 4096

#define PATH "/pmem-fs/myfile"

int
main(int argc, char *argv[])
{
	char *pmemaddr;
	size_t mapped_len;
	int is_pmem;

	/* create a pmem file and memory map it */

	if ((pmemaddr = pmem_map_file!U{}(PATH, PMEM_LEN, PMEM_FILE_CREATE,
			0666, &mapped_len, &is_pmem)) == NULL) {
		perror("pmem_map_file!U");
		exit(1);
	}

	/* store a string to the persistent memory */
	strcpy(pmemaddr, "hello, persistent memory");

	/* flush above strcpy to persistence */
	if (is_pmem)
		pmem_persist(pmemaddr, mapped_len);
	else
		pmem_msync(pmemaddr, mapped_len);

	/*
	 * Delete the mappings. The region is also
	 * automatically unmapped when the process is
	 * terminated.
	 */
	pmem_unmap(pmemaddr, mapped_len);
}
```

See <http://pmem.io/nvml/libpmem>
for more examples using the **libpmem** API.


# ACKNOWLEDGEMENTS #

**libpmem** builds on the persistent memory programming model
recommended by the SNIA NVM Programming Technical Work Group:
<http://snia.org/nvmp>


# SEE ALSO #

**pmem_flush**(3), **pmem_is_pmem**(3), **pmem_memmove_persist**(3),
**pmem_msync**(3), **pmem_persist**(3), **strerror**(3),
**libpmemobj**(7), **libpmemblk**(7), **libpmemlog**(7)
and **<http://pmem.io>**

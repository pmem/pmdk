---
layout: manual
Content-Style: 'text/css'
title: libpmem
header: NVM Library
date: pmem API version 1.0.4
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

[comment]: <> (libpmem.3 -- man page for libpmem)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[MOST COMMONLY USED FUNCTIONS](#most-commonly-used-functions-1)<br />
[PARTIAL FLUSHING OPERATIONS](#partial-flushing-operations-1)<br />
[COPYING TO PERSISTENT MEMORY](#copying-to-persistent-memory-1)<br />
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

##### Most commonly used functions: #####

```c
int pmem_is_pmem(const void *addr, size_t len);
void pmem_persist(const void *addr, size_t len);
int pmem_msync(const void *addr, size_t len);
void *pmem_map_file(const char *path, size_t len, int flags,
	mode_t mode, size_t *mapped_lenp, int *is_pmemp);
int pmem_unmap(void *addr, size_t len);
```

##### Partial flushing operations: #####

```c
void pmem_flush(const void *addr, size_t len);
void pmem_drain(void);
int pmem_has_hw_drain(void);
```

##### Copying to persistent memory: #####

```c
void *pmem_memmove_persist(void *pmemdest, const void *src, size_t len);
void *pmem_memcpy_persist(void *pmemdest, const void *src, size_t len);
void *pmem_memset_persist(void *pmemdest, int c, size_t len);
void *pmem_memmove_nodrain(void *pmemdest, const void *src, size_t len);
void *pmem_memcpy_nodrain(void *pmemdest, const void *src, size_t len);
void *pmem_memset_nodrain(void *pmemdest, int c, size_t len);
```

##### Library API versioning: #####

```c
const char *pmem_check_version(
	unsigned major_required,
	unsigned minor_required);
```

##### Error handling: #####

```c
const char *pmem_errormsg(void);
```


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

+ **libpmemobj**(3), a general use persistent memory API, providing memory
  allocation and transactional operations on variable-sized objects.

+ **libpmemblk**(3), providing pmem-resident arrays of fixed-sized blocks
  with atomic updates.

+ **libpmemlog**(3), providing a pmem-resident log file.

Under normal usage, **libpmem** will never print messages or
intentionally cause the process to exit.
The only exception to this is the debugging information, when enabled,
as described under **DEBUGGING AND ERROR HANDLING** below.


# MOST COMMONLY USED FUNCTIONS #

Most pmem-aware applications will take advantage of higher level
libraries that alleviate the application from calling into **libpmem**
directly. Application developers that wish to access raw memory mapped
persistence directly (via **mmap**(2)) and that wish to take on the
responsibility for flushing stores to persistence will find the
functions described in this section to be the most commonly used.

```c
int pmem_is_pmem(const void *addr, size_t len);
```

The **pmem_is_pmem**() function returns true only if the entire range
\[*addr*, *addr*+*len*) consists of persistent memory. A true return
from **pmem_is_pmem**() means it is safe to use **pmem_persist**()
and the related functions below to make changes durable for that memory
range.

The implementation of **pmem_is_pmem**() requires a non-trivial amount
of work to determine if the given range is entirely persistent memory.
For this reason, it is better to call **pmem_is_pmem**() once when a
range of memory is first encountered, save the result, and use the saved
result to determine whether **pmem_persist**() or **msync**(2) is
appropriate for flushing changes to persistence. Calling
**pmem_is_pmem**() each time changes are flushed to persistence will
not perform well.

>NOTE:
On Linux, **pmem_is_pmem**() returns true only if the entire range
is mapped directly from Device DAX (/dev/daxX.Y) without an intervening
file system.  In the future, as file systems become available that support
flushing with **pmem_persist**(), **pmem_is_pmem**() will return true
as appropriate.

>WARNING:
Using **pmem_persist**() on a range where **pmem_is_pmem**()
returns false may not do anything useful -- use **msync**(2) instead.

```c
void pmem_persist(const void *addr, size_t len);
```

Force any changes in the range \[*addr*, *addr*+*len*) to be stored
durably in persistent memory. This is equivalent to calling **msync**(2)
but may be more optimal and will avoid calling into the kernel if
possible. There are no alignment restrictions on the range described by
*addr* and *len*, but **pmem_persist**() may expand the range as
necessary to meet platform alignment requirements.

>WARNING:
Like **msync**(2), there is nothing atomic or transactional
about this call. Any unwritten stores in the given range will be
written, but some stores may have already been written by virtue of
normal cache eviction/replacement policies. Correctly written code must
not depend on stores waiting until **pmem_persist**() is called to
become persistent -- they can become persistent at any time before
**pmem_persist**() is called.

```c
int pmem_msync(const void *addr, size_t len);
```

The function **pmem_msync**() is like **pmem_persist**() in that it
forces any changes in the range \[*addr*, *addr*+*len*) to be stored
durably. Since it calls **msync**(), this function works on either
persistent memory or a memory mapped file on traditional storage.
**pmem_msync**() takes steps to ensure the alignment of addresses and
lengths passed to **msync**() meet the requirements of that system call.
It calls **msync**() with the **MS_SYNC** flag as described in
**msync**(2). Typically the application only checks for the existence of
persistent memory once, and then uses that result throughout the
program, for example:

```c
/* do this call once, after the pmem is memory mapped */
int is_pmem = pmem_is_pmem(rangeaddr, rangelen);

/* ... make changes to a range of pmem ... */

/* make the changes durable */
if (is_pmem)
	pmem_persist(subrangeaddr, subrangelen);
else
	pmem_msync(subrangeaddr, subrangelen);

/* ... */
```

The return value of **pmem_msync**() is the return value of
**msync**(), which can return -1 and set *errno* to indicate an error.

>WARNING:
On Linux, **pmem_msync**() and **msync**(2) have no effect on memory ranges
mapped from Device DAX.  In case of memory ranges where **pmem_is_pmem**()
returns true use **pmem_persist**() to force the changes to be stored durably
in persistent memory.

```c
void *pmem_map_file(const char *path, size_t len, int flags,
	mode_t mode, size_t *mapped_lenp, int *is_pmemp);
```

Given a *path*, **pmem_map_file**() function creates a new read/write
mapping for the named file. It will map the file using **mmap**(2), but
it also takes extra steps to make large page mappings more likely.

On success, **pmem_map_file**() returns a pointer to mapped area. If
*mapped_lenp* is not NULL, the length of the mapping is also stored at
the address it points to. The *is_pmemp* argument, if non-NULL, points
to a flag that **pmem_is_pmem**() sets to say if the mapped file is
actual pmem, or if **msync**() must be used to flush writes for the
mapped range. On error, NULL is returned, *errno* is set appropriately,
and *mapped_lenp* and *is_pmemp* are left untouched.

The *flags* argument can be 0 or bitwise OR of one or more of the
following file creation flags:

+ **PMEM_FILE_CREATE** - Create the named file if it does not exist.
  *len* must be non-zero and specifies the size of the file to be created.
  *mode* has the same meaning as for **open**(2) and specifies the mode to
  use in case a new file is created. If neither **PMEM_FILE_CREATE** nor
  **PMEM_FILE_TMPFILE** is specified, then *mode* is ignored.

+ **PMEM_FILE_EXCL** - Same meaning as **O_EXCL** on **open**(2) -
  Ensure that this call creates the file. If this flag is specified in
  conjunction with **PMEM_FILE_CREATE**, and pathname already exists,
  then **pmem_map_file**() will fail.

+ **PMEM_FILE_TMPFILE** - Same meaning as **O_TMPFILE** on **open**(2).
  Create a mapping for an unnamed temporary file. **PMEM_FILE_CREATE**
  and *len* must be specified and *path* must be an existing directory
  name.

+ **PMEM_FILE_SPARSE** - When creating a file, create a sparse (holey)
  file instead of calling **posix_fallocate**(2). Valid only if specified
  in conjunction with **PMEM_FILE_CREATE** or **PMEM_FILE_TMPFILE**,
  otherwise ignored.

If creation flags are not supplied, then **pmem_map_file**() creates a
mapping for an existing file. In such case, *len* should be zero. The
entire file is mapped to memory; its length is used as the length of the
mapping and returned via *mapped_lenp*.

The path of a file can point to a Device DAX and in such case only
**PMEM_FILE_CREATE** and **PMEM_FILE_SPARSE** flags are valid, but they both
effectively do nothing. For Device DAX mappings, the *len* argument must be,
regardless of the flags, equal to either 0 or the exact size of the device.

To delete mappings created with **pmem_map_file**(), use **pmem_unmap**().

```c
int pmem_unmap(void *addr, size_t len);
```

The **pmem_unmap**() function deletes all the mappings for the
specified address range, and causes further references to addresses
within the range to generate invalid memory references. It will use the
address specified by the parameter *addr*, where *addr* must be a
previously mapped region. **pmem_unmap**() will delete the mappings
using the **munmap**(2), On success, **pmem_unmap**() returns zero. On
error, -1 is returned, and *errno* is set appropriately.


# PARTIAL FLUSHING OPERATIONS #

The functions in this section provide access to the stages of flushing
to persistence, for the less common cases where an application needs
more control of the flushing operations than the **pmem_persist**()
function described above.

```c
void pmem_flush(const void *addr, size_t len);
void pmem_drain(void);
```

These functions provide partial versions of the **pmem_persist**()
function described above. **pmem_persist**() can be thought of as this:

```c
void
pmem_persist(const void *addr, size_t len)
{
	/* flush the processor caches */
	pmem_flush(addr, len);

	/* wait for any pmem stores to drain from HW buffers */
	pmem_drain();
}
```

These functions allow advanced programs to create their own variations
of **pmem_persist**(). For example, a program that needs to flush
several discontiguous ranges can call **pmem_flush**() for each range
and then follow up by calling **pmem_drain**() once.

```c
int pmem_has_hw_drain(void);
```

The **pmem_has_hw_drain**() function returns true if the machine
supports an explicit *hardware drain*
instruction for persistent memory.  On Intel processors with
persistent memory, stores to persistent memory are considered
persistent once they are flushed from the CPU caches, so this
function always returns false.  Despite that, programs using
**pmem_flush**() to flush ranges of memory should still follow up by calling
**pmem_drain**() once to ensure the flushes are complete.  As mentioned above,
**pmem_persist**() handles calling both **pmem_flush**() and **pmem_drain**().


# COPYING TO PERSISTENT MEMORY #

The functions in this section provide optimized copying to persistent
memory.

```c
void *pmem_memmove_persist(void *pmemdest, const void *src, size_t len);
void *pmem_memcpy_persist(void *pmemdest, const void *src, size_t len);
void *pmem_memset_persist(void *pmemdest, int c, size_t len);
```

The **pmem_memmove_persist**(), **pmem_memcpy_persist**(), and
**pmem_memset_persist**(), functions provide the same memory copying
as their namesakes **memmove**(3), **memcpy**(3) and **memset**(3), and
ensure that the result has been flushed to persistence before returning.
For example, the following code is functionally equivalent to
**pmem_memmove_persist**():

```c
void *
pmem_memmove_persist(void *pmemdest, const void *src, size_t len)
{
	void *retval = memmove(pmemdest, src, len);

	pmem_persist(pmemdest, len);

	return retval;
}
```

Calling **pmem_memmove_persist**() may out-perform the above code,
however, since the **libpmem** implementation may take advantage of the
fact that *pmemdest* is persistent memory and use instructions such as
*non-temporal* stores to avoid the need to flush processor caches.

>WARNING:
Using these functions where **pmem_is_pmem**() returns false
may not do anything useful. Use the normal libc functions in that case.

```c
void *pmem_memmove_nodrain(void *pmemdest, const void *src, size_t len);
void *pmem_memcpy_nodrain(void *pmemdest, const void *src, size_t len);
void *pmem_memset_nodrain(void *pmemdest, int c, size_t len);
```

The **pmem_memmove_nodrain**(), **pmem_memcpy_nodrain**() and
**pmem_memset_nodrain**() functions are similar to
**pmem_memmove_persist**(), **pmem_memcpy_persist**(), and
**pmem_memset_persist**() described above, except they skip the final
**pmem_drain**() step. This allows applications to optimize cases where
several ranges are being copied to persistent memory, followed by a
single call to **pmem_drain**(). The following example illustrates how
these functions might be used to avoid multiple calls to
**pmem_drain**() when copying several ranges of memory to pmem:

```c
/* ... write several ranges to pmem ... */
pmem_memcpy_nodrain(pmemdest1, src1, len1);
pmem_memcpy_nodrain(pmemdest2, src2, len2);

/* ... */

/* wait for any pmem stores to drain from HW buffers */
pmem_drain();
```

>WARNING:
Using **pmem_memmove_nodrain**(), **pmem_memcpy_nodrain**()
or **pmem_memset_nodrain**() on a destination where
**pmem_is_pmem**() returns false may not do anything useful.


# LIBRARY API VERSIONING #

This section describes how the library API is versioned, allowing
applications to work with an evolving API.

```c
const char *pmem_check_version(
	unsigned major_required,
	unsigned minor_required);
```

The **pmem_check_version**() function is used to see if the installed
**libpmem** supports the version of the library API required by an
application. The easiest way to do this is for the application to supply
the compile-time version information, supplied by defines in
**\<libpmem.h\>**, like this:

```c
reason = pmem_check_version(PMEM_MAJOR_VERSION,
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

When the version check performed by **pmem_check_version**() is
successful, the return value is NULL. Otherwise the return value is a
static string describing the reason for failing the version check. The
string returned by **pmem_check_version**() must not be modified or
freed.


# DEBUGGING AND ERROR HANDLING #

Two versions of **libpmem** are typically available on a development
system. The normal version, accessed when a program is linked using the
**-lpmem** option, is optimized for performance. That version skips
checks that impact performance and never logs any trace information or
performs any run-time assertions. If an error is detected during the
call to **libpmem** function, an application may retrieve an error
message describing the reason of failure using the following function:

```c
const char *pmem_errormsg(void);
```

The **pmem_errormsg**() function returns a pointer to a static buffer
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

A second version of **libpmem**, accessed when a program uses the
libraries under **/usr/lib/nvml_debug**, contains run-time assertions
and trace points. The typical way to access the debug version is to set
the environment variable **LD_LIBRARY_PATH** to
**/usr/lib/nvml_debug** or **/usr/lib64/nvml_debug** depending on
where the debug libraries are installed on the system. The trace points
in the debug version of the library are enabled using the environment
variable **PMEM_LOG_LEVEL**, which can be set to the following values:

+ **0** - This is the default level when **PMEM_LOG_LEVEL** is not set.
  No log messages are emitted at this level.

+ **1** - Additional details on any errors detected are logged (in addition
  to returning the *errno*-based errors as usual). The same information
  may be retrieved using **pmem_errormsg**().

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

If *val* is 0 (zero), then **pmem_is_pmem**() will always return
false. Setting *val* to 1 causes **pmem_is_pmem**() to always return
true. This variable is mostly used for testing but can be used to force
pmem behavior on a system where a range of pmem is not detectable as
pmem for some reason.

>NOTE:
Unlike the other variables, the value of
**PMEM_IS_PMEM_FORCE** is not queried (and cached) at the
library initialization time, but on the first call to
**pmem_is_pmem**() function. It means that in case of
**libpmemlog**, **libpmemblk**, and **libpmemobj**
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
the hint address used by **pmem_map_file**(). If set, it also disables
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
There is nothing transactional about the **pmem_persist**() or
**pmem_msync**() calls in this example. Interrupting the program may
result in a partial write to pmem. Use a transactional library such as
**libpmemobj**(3) to avoid torn updates.

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

	if ((pmemaddr = pmem_map_file(PATH, PMEM_LEN, PMEM_FILE_CREATE,
			0666, &mapped_len, &is_pmem)) == NULL) {
		perror("pmem_map_file");
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

**libpmemobj**(3), **libpmemblk**(3), **libpmemlog**(3), **libvmem**(3),
**open**(2), **mmap**(2), **munmap**(2), **msync**(2), **strerror**(3)
and **<http://pmem.io>**

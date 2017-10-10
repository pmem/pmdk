---
layout: manual
Content-Style: 'text/css'
title: LIBPMEMLOG
collection: libpmemlog
header: NVM Library
date: pmemlog API version 1.0
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

[comment]: <> (libpmemlog.3 -- man page for libpmemlog)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[MOST COMMONLY USED FUNCTIONS](#most-commonly-used-functions-1)<br />
[LIBRARY API VERSIONING](#library-api-versioning-1)<br />
[MANAGING LIBRARY BEHAVIOR](#managing-library-behavior-1)<br />
[DEBUGGING AND ERROR HANDLING](#debugging-and-error-handling)<br />
[EXAMPLE](#example)<br />
[BUGS](#bugs)<br />
[ACKNOWLEDGEMENTS](#acknowledgements)<br />
[SEE ALSO](#see-also)


# NAME #

**libpmemlog** -- persistent memory resident log file


# SYNOPSIS #

```c
#include <libpmemlog.h>
cc ... -lpmemlog -lpmem
```

>NOTE: NVML API supports UNICODE. If **NVML_UTF8_API** macro is defined then
basic API functions are expanded to UTF-8 API with postfix *U*,
otherwise they are expanded to UNICODE API with postfix *W*.

##### Most commonly used functions: #####

```c
PMEMlogpool *pmemlog_openU(const char *path);
PMEMlogpool *pmemlog_openW(const wchar_t *path);
PMEMlogpool *pmemlog_createU(const char *path, size_t poolsize, mode_t mode);
PMEMlogpool *pmemlog_createW(const wchar_t *path, size_t poolsize, mode_t mode);
void pmemlog_close(PMEMlogpool *plp);
size_t pmemlog_nbyte(PMEMlogpool *plp);
intpmemlog_append(PMEMlogpool *plp, const void *buf, size_t count);
int pmemlog_appendv(PMEMlogpool *plp, const struct iovec *iov, int iovcnt);
long long pmemlog_tell(PMEMlogpool *plp);
void pmemlog_rewind(PMEMlogpool *plp);
void pmemlog_walk(PMEMlogpool *plp, size_t chunksize,
	int (*process_chunk)(const void *buf, size_t len, void *arg),
	void *arg);
```

##### Library API versioning: #####

```c
const char *pmemlog_check_versionU(
	unsigned major_required,
	unsigned minor_required);
const wchar_t *pmemlog_check_versionW(
	unsigned major_required,
	unsigned minor_required);
```

##### Managing library behavior: #####

```c
void pmemlog_set_funcs(
	void *(*malloc_func)(size_t size),
	void (*free_func)(void *ptr),
	void *(*realloc_func)(void *ptr, size_t size),
	char *(*strdup_func)(const char *s));
int pmemlog_checkU(const char *path);
	int pmemlog_checkW(const wchar_t *path);
```

##### Error handling: #####

```c
const char *pmemlog_errormsgU(void);
const wchar_t *pmemlog_errormsgW(void);
```


# DESCRIPTION #

**libpmemlog**
provides a log file in *persistent memory* (pmem) such that additions to the log are appended atomically. This library is intended for applications using
direct access storage (DAX), which is storage that supports load/store access without paging blocks from a block storage device. Some types of *non-volatile
memory DIMMs* (NVDIMMs) provide this type of byte addressable access to storage. A *persistent memory aware file system* is typically used to expose the direct
access to applications. Memory mapping a file from this type of file system results in the load/store, non-paged access to pmem. **libpmemlog** builds on this
type of memory mapped file.

This library is for applications that need a persistent log file, updated atomically (the updates cannot be *torn* by program interruption such as power
failures). This library builds on the low-level pmem support provided by **libpmem**(3), handling the transactional update of the log, flushing to persistence,
and recovery for the application.

**libpmemlog** is one of a collection of persistent memory libraries available, the others are:

+ **libpmemobj**(3), a general use persistent memory API, providing memory allocation and transactional operations on variable-sized objects.

+ **libpmemblk**(3), providing pmem-resident arrays of fixed-sized blocks with atomic updates.

+ **libpmem**(3), low-level persistent memory support.

Under normal usage, **libpmemlog** will never print messages or intentionally cause the process to exit. The only exception to this is the debugging
information, when enabled, as described under **DEBUGGING AND ERROR HANDLING** below.


# MOST COMMONLY USED FUNCTIONS #

To use the pmem-resident log file provided by **libpmemlog**, a *memory pool* is first created. This is done with the **pmemlog_createU**()/**pmemlog_createW**() function described
in this section. The other functions described in this section then operate on the resulting log memory pool.

Once created, the memory pool is represented by an opaque handle, of type *PMEMlogpool\**, which is passed to most of the other functions in this section.
Internally, **libpmemlog** will use either **pmem_persist**() or **msync**(2) when it needs to flush changes, depending on whether the memory pool appears to
be persistent memory or a regular file (see the **pmem_is_pmem**() function in **libpmem**(3) for more information). There is no need for applications to flush
changes directly when using the log memory API provided by **libpmemlog**.

```c
PMEMlogpool *pmemlog_openU(const char *path);
PMEMlogpool *pmemlog_openW(const wchar_t *path);
```

The **pmemlog_openU**()/**pmemlog_openW**() function opens an existing log memory pool, returning a memory pool handle used with most of the functions in this section. *path* must
be an existing file containing a log memory pool as created by **pmemlog_createU**()/**pmemlog_createW**(). The application must have permission to open the file and memory map it
with read/write permissions. If an error prevents the pool from being opened, **pmemlog_openU**()/**pmemlog_openW**() returns NULL and sets *errno* appropriately.

```c
PMEMlogpool *pmemlog_createU(const char *path, size_t poolsize, mode_t mode);
PMEMlogpool *pmemlog_createW(const wchar_t *path, size_t poolsize, mode_t mode);
```

The **pmemlog_createU**()/**pmemlog_createW**() function creates a log memory pool with the given total *poolsize*. Since the transactional nature of a log memory pool requires some
space overhead in the memory pool, the resulting available log size is less than *poolsize*, and is made available to the caller via the **pmemlog_nbyte**()
function described below. *path* specifies the name of the memory pool file to be created. *mode* specifies the permissions to use when creating the file as
described by **creat**(2). The memory pool file is fully allocated to the size *poolsize* using **posix_fallocate**(3). The caller may choose to take
responsibility for creating the memory pool file by creating it before calling **pmemlog_createU**()/**pmemlog_createW**() and then specifying *poolsize* as zero. In this case
**pmemlog_createU**()/**pmemlog_createW**() will take the pool size from the size of the existing file and will verify that the file appears to be empty by searching for any non-zero
data in the pool header at the beginning of the file. The minimum file size allowed by the library for a log pool is defined in **\<libpmemlog.h\>** as
**PMEMLOG_MIN_POOL**.

Depending on the configuration of the system, the available space of non-volatile memory space may be divided into multiple memory devices. In such case, the
maximum size of the pmemlog memory pool could be limited by the capacity of a single memory device. The **libpmemlog** allows building persistent memory
resident log spanning multiple memory devices by creation of persistent memory pools consisting of multiple files, where each part of such a *pool set* may be
stored on different pmem-aware filesystem.

Creation of all the parts of the pool set can be done with the **pmemlog_createU**()/**pmemlog_createW**() function. However, the recommended method for creating pool sets is to do
it by using the **pmempool**(1) utility.

When creating the pool set consisting of multiple files, the *path* argument passed to **pmemlog_createU**()/**pmemlog_createW**() must point to the special *set* file that defines
the pool layout and the location of all the parts of the pool set. The *poolsize* argument must be 0. The meaning of *layout* and *mode* arguments doesn't
change, except that the same *mode* is used for creation of all the parts of the pool set. If the error prevents any of the pool set files from being created,
**pmemlog_createU**()/**pmemlog_createW**() returns NULL and sets *errno* appropriately.

When opening the pool set consisting of multiple files, the *path* argument passed to **pmemlog_openU**()/**pmemlog_openW**() must not point to the pmemlog memory pool file, but to
the same *set* file that was used for the pool set creation. If an error prevents any of the pool set files from being opened, or if the actual size of any
file does not match the corresponding part size defined in *set* file **pmemlog_openU**()/**pmemlog_openW**() returns NULL and sets *errno* appropriately.

The set file is a plain text file, which must start with the line containing a *PMEMPOOLSET* string, followed by the specification of all the pool parts in the
next lines. For each part, the file size and the absolute path must be provided.

The size has to be compliant with the format specified in IEC 80000-13, IEEE 1541 or the Metric Interchange Format.  Standards accept SI units with obligatory
B - kB, MB, GB, ... (multiplier by 1000) and IEC units with optional "iB" - KiB, MiB, GiB, ..., K, M, G, ... - (multiplier by 1024).

The path of a part can point to a Device DAX and in such case the size
argument can be set to an "AUTO" string, which means that the size of the device
will be automatically resolved at pool creation time.
When using Device DAX there's also one additional restriction - it is not allowed
to concatenate more than one Device DAX device in a single pool set
if the configured internal alignment is other than 4KiB.  In such case a pool set
can consist only of a single part (single Device DAX).
Please see **ndctl-create-namespace**(1) for information on how to configure
desired alignment on Device DAX.

Device DAX is the device-centric analogue of Filesystem DAX. It allows memory
ranges to be allocated and mapped without need of an intervening file system.
For more information please see **ndctl-create-namespace**(1).

The minimum file size of each part of the pool set is the same as the minimum size allowed for a log pool consisting of one file. It is defined in
**\<libpmemlog.h\>** as **PMEMLOG_MIN_POOL**. Lines starting with "#" character are ignored.

Here is the example "mylogpool.set" file:

```
PMEMPOOLSET
100G /mountpoint0/myfile.part0
200G /mountpoint1/myfile.part1
400G /mountpoint2/myfile.part2
```

The files in the set may be created by running the following command:

```
$ pmempool create log mylogpool.set
```

```c
void pmemlog_close(PMEMlogpool *plp);
```

The **pmemlog_close**() function closes the memory pool indicated by *plp* and deletes the memory pool handle. The log memory pool itself lives on in the file
that contains it and may be re-opened at a later time using **pmemlog_openU**()/**pmemlog_openW**() as described above.

```c
size_t pmemlog_nbyte(PMEMlogpool *plp);
```

The **pmemlog_nbyte**() function returns the amount of usable space in the log *plp*. This function may be used on a log to determine how much usable space is
available after **libpmemlog** has added its metadata to the memory pool.

```c
int pmemlog_append(PMEMlogpool *plp, const void *buf, size_t count);
```

The **pmemlog_append**() function appends *count* bytes from *buf* to the current write offset in the log memory pool *plp*. Calling this function is analogous
to appending to a file. The append is atomic and cannot be torn by a program failure or system crash. On success, zero is returned. On error, -1 is returned
and *errno* is set.

```c
int pmemlog_appendv(PMEMlogpool *plp, const struct iovec *iov, int iovcnt);
```

The **pmemlog_appendv**() function appends to the log *plp* just like **pmemlog_append**() above, but this function takes a scatter/gather list in a manner
similar to **writev**(2). In this case, the entire list of buffers is appended atomically, as if the buffers in *iov* were concatenated in order. On success,
zero is returned. On error, -1 is returned and *errno* is set.

>NOTE:
Since **libpmemlog** is designed as a low-latency code path, many of the checks routinely done by the operating system for **writev**(2) are not
practical in the library's implementation of **pmemlog_appendv**(). No attempt is made to detect NULL or incorrect pointers, or illegal count values, for
example.

```c
long long pmemlog_tell(PMEMlogpool *plp);
```

The **pmemlog_tell**() function returns the current write point for the log, expressed as a byte offset into the usable log space in the memory pool. This
offset starts off as zero on a newly-created log, and is incremented by each successful append operation. This function can be used to determine how much data
is currently in the log.

```c
void pmemlog_rewind(PMEMlogpool *plp);
```

The **pmemlog_rewind**() function resets the current write point for the log to zero. After this call, the next append adds to the beginning of the log.

```c
void pmemlog_walk(PMEMlogpool *plp, size_t chunksize,
	int (*process_chunk)(const void *buf, size_t len, void *arg),
	void *arg);
```

The **pmemlog_walk**() function walks through the log *plp*, from beginning to end, calling the callback function *process_chunk* for each *chunksize* block of
data found. The argument *arg* is also passed to the callback to help avoid the need for global state. The *chunksize* argument is useful for logs with
fixed-length records and may be specified as 0 to cause a single call to the callback with the entire log contents passed as the *buf* argument. The *len*
argument tells the *process_chunk* function how much data buf is holding. The callback function should return 1 if **pmemlog_walk**() should continue walking
through the log, or 0 to terminate the walk. The callback function is called while holding **libpmemlog** internal locks that make calls atomic, so the
callback function must not try to append to the log itself or deadlock will occur.


# CAVEATS #

**libpmemlog** relies on the library destructor being called from the main
thread. For this reason, all functions that might trigger destruction (e.g.
**dlclose**()) should be called in the main thread. Otherwise some of the
resources associated with that thread might not be cleaned up properly.


# LIBRARY API VERSIONING #

This section describes how the library API is versioned, allowing applications to work with an evolving API.

```c
const char *pmemlog_check_versionU(
	unsigned major_required,
	unsigned minor_required);
const wchar_t *pmemlog_check_versionW(
	unsigned major_required,
	unsigned minor_required);
```

The **pmemlog_check_versionU**()/**pmemlog_check_versionW**() function is used to see if the installed **libpmemlog** supports the version of the library API required by an application. The
easiest way to do this is for the application to supply the compile-time version information, supplied by defines in **\<libpmemlog.h\>**, like this:

```c
reason = pmemlog_check_versionU(PMEMLOG_MAJOR_VERSION,
                               PMEMLOG_MINOR_VERSION);
if (reason != NULL) {
	/* version check failed, reason string tells you why */
}
```

Any mismatch in the major version number is considered a failure, but a library with a newer minor version number will pass this check since increasing minor
versions imply backwards compatibility.

An application can also check specifically for the existence of an interface by checking for the version where that interface was introduced. These versions
are documented in this man page as follows: unless otherwise specified, all interfaces described here are available in version 1.0 of the library. Interfaces
added after version 1.0 will contain the text *introduced in version x.y* in the section of this manual describing the feature.

When the version check performed by **pmemlog_check_versionU**()/**pmemlog_check_versionW**() is successful, the return value is NULL. Otherwise the return value is a static string
describing the reason for failing the version check. The string returned by **pmemlog_check_versionU**()/**pmemlog_check_versionW**() must not be modified or freed.


# MANAGING LIBRARY BEHAVIOR #

The library entry points described in this section are less commonly used than the previous sections.

```c
void pmemlog_set_funcs(
	void *(*malloc_func)(size_t size),
	void (*free_func)(void *ptr),
	void *(*realloc_func)(void *ptr, size_t size),
	char *(*strdup_func)(const char *s));
```

The **pmemlog_set_funcs**() function allows an application to override memory allocation calls used internally by **libpmemlog**. Passing in NULL for any of
the handlers will cause the **libpmemlog** default function to be used. The library does not make heavy use of the system malloc functions, but it does
allocate approximately 4-8 kilobytes for each memory pool in use.

```c
int pmemlog_checkU(const char *path);
	int pmemlog_checkW(const wchar_t *path);
```

The **pmemlog_checkU**()/**pmemlog_checkW**() function performs a consistency check of the file indicated by *path* and returns 1 if the memory pool is found to be consistent. Any
inconsistencies found will cause **pmemlog_checkU**()/**pmemlog_checkW**() to return 0, in which case the use of the file with **libpmemlog** will result in undefined behavior. The
debug version of **libpmemlog** will provide additional details on inconsistencies when **PMEMLOG_LOG_LEVEL** is at least 1, as described in the **DEBUGGING AND
ERROR HANDLING** section below. **pmemlog_checkU**()/**pmemlog_checkW**() will return -1 and set *errno* if it cannot perform the consistency check due to other errors.
**pmemlog_checkU**()/**pmemlog_checkW**() opens the given *path* read-only so it never makes any changes to the file. This function is not supported on Device DAX.


# DEBUGGING AND ERROR HANDLING #

Two versions of **libpmemlog** are typically available on a development system. The normal version, accessed when a program is linked using the **-lpmemlog**
option, is optimized for performance. That version skips checks that impact performance and never logs any trace information or performs any run-time
assertions. If an error is detected during the call to **libpmemlog** function, an application may retrieve an error message describing the reason of failure
using the following function:

```c
const char *pmemlog_errormsgU(void);
const wchar_t *pmemlog_errormsgW(void);
```

The **pmemlog_errormsgU**()/**pmemlog_errormsgW**() function returns a pointer to a static buffer containing the last error message logged for current thread. The error message may
include description of the corresponding error code (if *errno* was set), as returned by **strerror**(3). The error message buffer is thread-local; errors
encountered in one thread do not affect its value in other threads. The buffer is never cleared by any library function; its content is significant only when
the return value of the immediately preceding call to **libpmemlog** function indicated an error, or if *errno* was set. The application must not modify or
free the error message string, but it may be modified by subsequent calls to other library functions.

A second version of **libpmemlog**, accessed when a program uses
the libraries under **/nvml/src/x64/Debug**, contains
run-time assertions and trace points. The typical way to
access the debug version is to set the environment variable
**LD_LIBRARY_PATH** to **/nvml/src/x64/Debug** or other location depending on where the debug
libraries are installed on the system.
The trace points in the debug version of the library are enabled using the environment
variable **PMEMLOG_LOG_LEVEL**, which can be set to the following values:

+ **0** - This is the default level when **PMEMLOG_LOG_LEVEL** is not set. No log messages are emitted at this level.

+ **1** - Additional details on any errors detected are logged (in addition to returning the *errno*-based errors as usual). The same information may be
retrieved using **pmemlog_errormsgU**()/**pmemlog_errormsgW**().

+ **2** - A trace of basic operations is logged.

+ **3** - This level enables a very verbose amount of function call tracing in the library.

+ **4** - This level enables voluminous and fairly obscure tracing information that is likely only useful to the **libpmemlog** developers.

The environment variable **PMEMLOG_LOG_FILE** specifies a file name where all logging information should be written. If the last character in the name is "-",
the PID of the current process will be appended to the file name when the log file is created. If **PMEMLOG_LOG_FILE** is not set, the logging output goes to
stderr.

Setting the environment variable **PMEMLOG_LOG_LEVEL** has no effect on the non-debug version of **libpmemlog**.
See also **libpmem**(3) to get information about other environment variables affecting **libpmemlog** behavior.


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
	plp = pmemlog_createU(path, POOL_SIZE, 0666);

	if (plp == NULL)
		plp = pmemlog_openU(path);

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

See <http://pmem.io/nvml/libpmemlog>
for more examples using the **libpmemlog** API.


# BUGS #

Unlike **libpmemobj**, data replication is not supported in **libpmemlog**.
Thus, it is not allowed to specify replica sections in pool set files.


# ACKNOWLEDGEMENTS #

**libpmemlog** builds on the persistent memory programming model recommended
by the SNIA NVM Programming Technical Work Group:
<http://snia.org/nvmp>


# SEE ALSO #

**mmap**(2), **munmap**(2), **msync**(2), **strerror**(3), **libpmemobj**(3),
**libpmemblk**(3), **libpmem**(3), **libvmem**(3), **ndctl-create-namespace**(1)
and **<http://pmem.io>**

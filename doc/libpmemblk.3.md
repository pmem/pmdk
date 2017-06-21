---
layout: manual
Content-Style: 'text/css'
title: LIBPMEMBLK!3
header: NVM Library
date: pmemblk API version 1.0
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

[comment]: <> (libpmemblk.3 -- man page for libpmemblk)

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
[SEE ALSO](#see-also)<br />


# NAME #

**libpmemblk** -- persistent memory resident array of blocks


# SYNOPSIS #

```c
#include <libpmemblk.h>
cc ... -lpmemblk -lpmem
```

!ifdef{WIN32}
{
>NOTE: NVML API supports UNICODE. If **NVML_UTF8_API** macro is defined then
basic API functions are expanded to UTF-8 API with postfix *U*,
otherwise they are expanded to UNICODE API with postfix *W*.
}

##### Most commonly used functions: #####

```c
!ifdef{WIN32}
{
PMEMblkpool *pmemblk_createU(const char *path, size_t bsize,
		size_t poolsize, mode_t mode);
PMEMblkpool *pmemblk_createW(const wchar_t *path, size_t bsize,
		size_t poolsize, mode_t mode);
PMEMblkpool *pmemblk_openU(const char *path, size_t bsize);
PMEMblkpool *pmemblk_openW(const wchar_t *path, size_t bsize);
}{
PMEMblkpool *pmemblk_create(const char *path, size_t bsize,
		size_t poolsize, mode_t mode);
PMEMblkpool *pmemblk_open(const char *path, size_t bsize);
}
void pmemblk_close(PMEMblkpool *pbp);
size_t pmemblk_bsize(PMEMblkpool *pbp);
size_t pmemblk_nblock(PMEMblkpool *pbp);
int pmemblk_read(PMEMblkpool *pbp, void *buf, long long blockno);
int pmemblk_write(PMEMblkpool *pbp, const void *buf, long long blockno);
int pmemblk_set_zero(PMEMblkpool *pbp, long long blockno);
int pmemblk_set_error(PMEMblkpool *pbp, long long blockno);
```

##### Library API versioning: #####

```c
!ifdef{WIN32}
{
const char *pmemblk_check_versionU(
	unsigned major_required,
	unsigned minor_required);
const wchar_t *pmemblk_check_versionW(
	unsigned major_required,
	unsigned minor_required);
}{
const char *pmemblk_check_version(
	unsigned major_required,
	unsigned minor_required);
}
```

##### Managing library behavior: #####

```c
void pmemblk_set_funcs(
	void *(*malloc_func)(size_t size),
	void (*free_func)(void *ptr),
	void *(*realloc_func)(void *ptr, size_t size),
	char *(*strdup_func)(const char *s));
!ifdef{WIN32}
{
int pmemblk_checkU(const char *path, size_t bsize);
int pmemblk_checkW(const wchar_t *path, size_t bsize);
}{
int pmemblk_check(const char *path, size_t bsize);
}
```

##### Error handling: #####

```c
!ifdef{WIN32}
{
const char *pmemblk_errormsgU(void);
const wchar_t *pmemblk_errormsgW(void);
}{
const char *pmemblk_errormsg(void);
}
```


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
builds on the low-level pmem support provided by **libpmem**(3), handling the
transactional update of the blocks, flushing to persistence, and recovery for
the application. **libpmemblk** is one of a collection of persistent memory
libraries available, the others are:

+ **libpmemobj**(3), a general use persistent memory API, providing memory
allocation and transactional operations on variable-sized objects.

+ **libpmemlog**(3), providing a pmem-resident log file.

+ **libpmem**(3), low-level persistent memory support.

Under normal usage, **libpmemblk** will never print messages or intentionally
cause the process to exit. The only exception to this is the debugging
information, when enabled, as described under **DEBUGGING AND ERROR HANDLING**
below.


# MOST COMMONLY USED FUNCTIONS #

To use the atomic block arrays supplied by **libpmemblk**, a *memory pool*
is first created. This is done with the !pmemblk_create function described
in this section. The other functions described in this section then operate
on the resulting block memory pool. Once created, the memory pool is represented
by an opaque handle, of type *PMEMblkpool\**, which is passed to most of the other
functions in this section. Internally, **libpmemblk** will use either
**pmem_persist**() or **msync**(2) when it needs to flush changes, depending
on whether the memory pool appears to be persistent memory or a regular file
(see the **pmem_is_pmem**() function in **libpmem**(3) for more information).
There is no need for applications to flush changes directly when using the
block memory API provided by **libpmemblk**.

```c
!ifdef{WIN32}
{
PMEMblkpool *pmemblk_openU(const char *path, size_t bsize);
PMEMblkpool *pmemblk_openW(const wchar_t *path, size_t bsize);
}{
PMEMblkpool *pmemblk_open(const char *path, size_t bsize);
}
```

The !pmemblk_open function opens an existing block memory pool, returning
a memory pool handle used with most of the functions in this section. *path*
must be an existing file containing a block memory pool as created by
!pmemblk_create. The application must have permission to open the file
and memory map it with read/write permissions. If the *bsize* provided is
non-zero, !pmemblk_open will verify the given block size matches the block
size used when the pool was created. Otherwise, !pmemblk_open will open
the pool without verification of the block size. The *bsize* can be determined
using the **pmemblk_bsize**() function. If an error prevents the pool from being
opened, !pmemblk_open returns NULL and sets *errno* appropriately.
A block size mismatch with the *bsize* argument passed in results in *errno*
being set to **EINVAL**.

```c
!ifdef{WIN32}
{
PMEMblkpool *pmemblk_createU(const char *path, size_t bsize,
		size_t poolsize, mode_t mode);
PMEMblkpool *pmemblk_createW(const wchar_t *path, size_t bsize,
		size_t poolsize, mode_t mode);
}{
PMEMblkpool *pmemblk_create(const char *path, size_t bsize,
		size_t poolsize, mode_t mode);
}
```

The !pmemblk_create function creates a block memory pool with the given total
*poolsize* divided up into as many elements of size *bsize* as will fit in the pool.
Since the transactional nature of a block memory pool requires some space overhead
in the memory pool, the resulting number of available blocks is less than
*poolsize*/*bsize*, and is made available to the caller via the **pmemblk_nblock**()
function described below. Given the specifics of the implementation, the number
of available blocks for the user cannot be less than 256. This translates to
at least 512 internal blocks. *path* specifies the name of the memory pool file
to be created. *mode* specifies the permissions to use when creating the file
as described by **creat**(2). The memory pool file is fully allocated to the size
*poolsize* using **posix_fallocate**(3). The caller may choose to take
responsibility for creating the memory pool file by creating it before calling
!pmemblk_create and then specifying *poolsize* as zero. In this case
!pmemblk_create will take the pool size from the size of the existing file
and will verify that the file appears to be empty by searching for any non-zero
data in the pool header at the beginning of the file. The minimum file size allowed
by the library for a block pool is defined in **\<libpmemblk.h\>** as **PMEMBLK_MIN_POOL**.
*bsize* can be any non-zero value, however **libpmemblk** will silently round up
the given size to **PMEMBLK_MIN_BLK**, as defined in **\<libpmemblk.h\>**.

Depending on the configuration of the system, the available space of non-volatile
memory space may be divided into multiple memory devices. In such case, the maximum
size of the pmemblk memory pool could be limited by the capacity of a single memory
device. The **libpmemblk** allows building persistent memory resident array spanning
multiple memory devices by creation of persistent memory pools consisting of multiple
files, where each part of such a *pool set* may be stored on different pmem-aware filesystem.

Creation of all the parts of the pool set can be done with the !pmemblk_create
function. However, the recommended method for creating pool sets is to do it by
using the **pmempool**(1) utility.

When creating the pool set consisting of multiple files, the *path* argument passed
to !pmemblk_create must point to the special *set* file that defines the pool
layout and the location of all the parts of the pool set. The *poolsize* argument
must be 0. The meaning of *layout* and *mode* arguments doesn't change, except that
the same *mode* is used for creation of all the parts of the pool set. If the error
prevents any of the pool set files from being created, !pmemblk_create returns
NULL and sets *errno* appropriately.

When opening the pool set consisting of multiple files, the *path* argument passed
to !pmemblk_open must not point to the pmemblk memory pool file, but to the same
*set* file that was used for the pool set creation. If an error prevents any of the
pool set files from being opened, or if the actual size of any file does not match
the corresponding part size defined in *set* file !pmemblk_open returns NULL
and sets *errno* appropriately.

The set file is a plain text file, which must start with the line containing
a *PMEMPOOLSET* string, followed by the specification of all the pool parts
in the next lines. For each part, the file size and the absolute path must be provided.

The size has to be compliant with the format specified in IEC 80000-13, IEEE 1541
or the Metric Interchange Format. Standards accept SI units with obligatory
B - kB, MB, GB, ... (multiplier by 1000) and IEC units with optional "iB"
- KiB, MiB, GiB, ..., K, M, G, ... - (multiplier by 1024).

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

The minimum file size of each part of the pool set is the same as the minimum size
allowed for a block pool consisting of one file. It is defined in **\<libpmemblk.h\>**
as **PMEMBLK_MIN_POOL**. Lines starting with "#" character are ignored.

Here is the example "myblkpool.set" file:

```
PMEMPOOLSET
100G /mountpoint0/myfile.part0
200G /mountpoint1/myfile.part1
400G /mountpoint2/myfile.part2
```

The files in the set may be created by running the following command:

```
$ pmempool create blk <bsize> myblkpool.set
```

```c
void pmemblk_close(PMEMblkpool *pbp);
```

The **pmemblk_close**() function closes the memory pool indicated by *pbp* and deletes the memory pool handle.
The block memory pool itself lives on in the file that contains it and may be re-opened at a later time using !pmemblk_open as described above.

```c
size_t pmemblk_bsize(PMEMblkpool *pbp);
```

The **pmemblk_bsize**() function returns the block size of the specified block memory pool. It's the value which was passed as *bsize* to !pmemblk_create.
*pbp* must be a block memory pool handle as returned by !pmemblk_open or !pmemblk_create.

```c
size_t pmemblk_nblock(PMEMblkpool *pbp);
```

The **pmemblk_nblock**() function returns the usable space in the block memory pool, expressed as the number of blocks available.
*pbp* must be a block memory pool handle as returned by !pmemblk_open or !pmemblk_create.

```c
int pmemblk_read(PMEMblkpool *pbp, void *buf, long long blockno);
```

The **pmemblk_read**() function reads a block from memory pool *pbp*, block number *blockno*, into the buffer *buf*.
On success, zero is returned. On error, -1 is returned and *errno* is set.
Reading a block that has never been written by **pmemblk_write**() will return a block of zeroes.

```c
int pmemblk_write(PMEMblkpool *pbp, const void *buf, long long blockno);
```

The **pmemblk_write**() function writes a block from *buf* to block number *blockno* in the memory pool *pbp*.
The write is atomic with respect to other reads and writes. In addition, the write cannot be torn by program failure or system crash;
on recovery the block is guaranteed to contain either the old data or the new data, never a mixture of both.
On success, zero is returned. On error, -1 is returned and *errno* is set.

```c
int pmemblk_set_zero(PMEMblkpool *pbp, long long blockno);
```

The **pmemblk_set_zero**() function writes zeros to block number *blockno* in memory pool *pbp*.
Using this function is faster than actually writing a block of zeros since **libpmemblk** uses metadata to indicate the block should read back as zero.
On success, zero is returned. On error, -1 is returned and *errno* is set.

```c
int pmemblk_set_error(PMEMblkpool *pbp, long long blockno);
```

The **pmemblk_set_error**() function sets the error state for block number *blockno* in memory pool *pbp*.
A block in the error state returns *errno* **EIO** when read. Writing the block clears the error state and returns the block to normal use.
On success, zero is returned. On error, -1 is returned and *errno* is set.


# CAVEATS #

**libpmemblk** relies on the library destructor being called from the main
thread. For this reason, all functions that might trigger destruction (e.g.
**dlclose**()) should be called in the main thread. Otherwise some of the
resources associated with that thread might not be cleaned up properly.


# LIBRARY API VERSIONING #

This section describes how the library API is versioned, allowing applications to work with an evolving API.

```c
!ifdef{WIN32}
{
const char *pmemblk_check_versionU(
	unsigned major_required,
	unsigned minor_required);
const wchar_t *pmemblk_check_versionW(
	unsigned major_required,
	unsigned minor_required);
}{
const char *pmemblk_check_version(
	unsigned major_required,
	unsigned minor_required);
}
```

The !pmemblk_check_version function is used to see if the installed **libpmemblk**
supports the version of the library API required by an application. The easiest way
to do this is for the application to supply the compile-time version information, supplied by defines in **\<ibpmemblk.h\>**, like this:

```c
reason = pmemblk_check_version!U{}(PMEMBLK_MAJOR_VERSION,
                               PMEMBLK_MINOR_VERSION);
if (reason != NULL) {
	/* version check failed, reason string tells you why */
}
```

Any mismatch in the major version number is considered a failure, but a library with
a newer minor version number will pass this check since increasing minor versions imply backwards compatibility.

An application can also check specifically for the existence of an interface
by checking for the version where that interface was introduced. These versions
are documented in this man page as follows: unless otherwise specified, all
interfaces described here are available in version 1.0 of the library.
Interfaces added after version 1.0 will contain the text *introduced in version x.y* in the section of this manual describing the feature.

When the version check performed by !pmemblk_check_version is successful,
the return value is NULL. Otherwise the return value is a static string describing
the reason for failing the version check. The string returned by !pmemblk_check_version must not be modified or freed.


# MANAGING LIBRARY BEHAVIOR #

The library entry points described in this section are less commonly used than the previous sections.

```c
void pmemblk_set_funcs(
	void *(*malloc_func)(size_t size),
	void (*free_func)(void *ptr),
	void *(*realloc_func)(void *ptr, size_t size),
	char *(*strdup_func)(const char *s));
```

The **pmemblk_set_funcs**() function allows an application to override memory allocation calls used internally by **libpmemblk**.
Passing in NULL for any of the handlers will cause the **libpmemblk** default function to be used.
The library does not make heavy use of the system malloc functions, but it does allocate approximately 4-8 kilobytes for each memory pool in use.

```c
!ifdef{WIN32}
{
int pmemblk_checkU(const char *path, size_t bsize);
int pmemblk_checkW(const wchar_t *path, size_t bsize);
}{
int pmemblk_check(const char *path, size_t bsize);
}
```

The !pmemblk_check function performs a consistency check of the file indicated by *path* and returns 1 if the memory pool is found to be consistent. Any
inconsistencies found will cause !pmemblk_check to return 0, in which case the use of the file with **libpmemblk** will result in undefined behavior. The
debug version of **libpmemblk** will provide additional details on inconsistencies when **PMEMBLK_LOG_LEVEL** is at least 1, as described in the **DEBUGGING AND
ERROR HANDLING** section below. When *bsize* is non-zero !pmemblk_check will compare it to the block size of the pool and return 0 when they don't
match. !pmemblk_check will return -1 and set *errno* if it cannot perform the consistency check due to other errors. !pmemblk_check opens the given
*path* read-only so it never makes any changes to the file. This function is not supported on Device DAX.


# DEBUGGING AND ERROR HANDLING #

Two versions of **libpmemblk** are typically available on a development system. The normal version, accessed when a program is linked using the **-lpmemblk**
option, is optimized for performance. That version skips checks that impact performance and never logs any trace information or performs any run-time
assertions. If an error is detected during the call to **libpmemblk** function, an application may retrieve an error message describing the reason of failure
using the following function:

```c
!ifdef{WIN32}
{
const char *pmemblk_errormsgU(void);
const wchar_t *pmemblk_errormsgW(void);
}{
const char *pmemblk_errormsg(void);
}
```

The !pmemblk_errormsg function returns a pointer to a static buffer containing the last error message logged for current thread. The error message may
include description of the corresponding error code (if *errno* was set), as returned by **strerror**(3). The error message buffer is thread-local; errors
encountered in one thread do not affect its value in other threads. The buffer is never cleared by any library function; its content is significant only when
the return value of the immediately preceding call to **libpmemblk** function indicated an error, or if *errno* was set. The application must not modify or
free the error message string, but it may be modified by subsequent calls to other library functions.

A second version of **libpmemblk**, accessed when a program uses the libraries under **/usr/lib/nvml_debug**, contains run-time assertions and trace points.
The typical way to access the debug version is to set the environment variable **LD_LIBRARY_PATH** to **/usr/lib/nvml_debug** or **/usr/lib64/nvml_debug**
depending on where the debug libraries are installed on the system. The trace points in the debug version of the library are enabled using the environment
variable **PMEMBLK_LOG_LEVEL**, which can be set to the following values:

+ **0** - This is the default level when **PMEMBLK_LOG_LEVEL** is not set. No log messages are emitted at this level.

+ **1** - Additional details on any errors detected are logged (in addition to returning the *errno*-based errors as usual). The same information may be
retrieved using !pmemblk_errormsg.

+ **2** - A trace of basic operations is logged.

+ **3** - This level enables a very verbose amount of function call tracing in the library.

+ **4** - This level enables voluminous and fairly obscure tracing information that is likely only useful to the **libpmemblk** developers.

The environment variable **PMEMBLK_LOG_FILE** specifies a file name where all
logging information should be written. If the last character in the name is "-",
the PID of the current process will be appended to the file name when the log
file is created. If **PMEMBLK_LOG_FILE** is not set, the logging output goes to stderr.

Setting the environment variable **PMEMBLK_LOG_LEVEL** has no effect on the non-debug version of **libpmemblk**.
See also **libpmem**(3) to get information about other environment variables affecting **libpmemblk** behavior.


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
	pbp = pmemblk_create!U{}(path, ELEMENT_SIZE, POOL_SIZE, 0666);

	if (pbp == NULL)
		pbp = pmemblk_open!U{}(path, ELEMENT_SIZE);

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

See <http://pmem.io/nvml/libpmemblk> for more examples using the **libpmemblk** API.


# BUGS #

Unlike **libpmemobj**, data replication is not supported in **libpmemblk**.
Thus, it is not allowed to specify replica sections in pool set files.


# ACKNOWLEDGEMENTS #

**libpmemblk** builds on the persistent memory programming model recommended
by the SNIA NVM Programming Technical Work Group:
<http://snia.org/nvmp>


# SEE ALSO #

**mmap**(2), **munmap**(2), **msync**(2), **strerror**(3), **libpmemobj**(3),
**libpmemlog**(3), **libpmem**(3), **libvmem**(3), **ndctl-create-namespace**(1)
and **<http://pmem.io>**

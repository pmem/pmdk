---
layout: manual
Content-Style: 'text/css'
title: LIBPMEMPOOL!3
header: NVM Library
date: pmempool API version 1.1
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

[comment]: <> (libpmempool.3 -- man page for libpmempool)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[POOL CHECKING FUNCTIONS](#pool-checking-functions)<br />
[POOL SET SYNCHRONIZATION AND TRANSFORMATION](#pool-set-synchronization-and-transformation-1)<br />
[POOL SET MANAGEMENT FUNCTIONS](#pool-set-management-functions)<br />
[LIBRARY API VERSIONING](#library-api-versioning-1)<br />
[DEBUGGING AND ERROR HANDLING](#debugging-and-error-handling)<br />
[EXAMPLE](#example)<br />
[ACKNOWLEDGEMENTS](#acknowledgements)<br />
[SEE ALSO](#see-also)<br />


# NAME #

**libpmempool** -- pool management library


# SYNOPSIS #

```c
#include <libpmempool.h>
cc -std=gnu99 ... -lpmempool -lpmem
```

!ifdef{WIN32}
{
>NOTE: NVML API supports UNICODE. If **NVML_UTF8_API** macro is defined then
basic API functions are expanded to UTF-8 API with postfix *U*,
otherwise they are expanded to UNICODE API with postfix *W*.
}

##### Health check functions: #####

```c
!ifdef{WIN32}
{
PMEMpoolcheck *pmempool_check_initU(struct pmempool_check_argsU *args,
	size_t args_size);
PMEMpoolcheck *pmempool_check_initW(struct pmempool_check_argsW *args,
	size_t args_size);
struct pmempool_check_statusU *pmempool_checkU(PMEMpoolcheck *ppc);
struct pmempool_check_statusW *pmempool_checkW(PMEMpoolcheck *ppc);
}{
PMEMpoolcheck *pmempool_check_init(struct pmempool_check_args *args,
	size_t args_size);
struct pmempool_check_status *pmempool_check(PMEMpoolcheck *ppc);
}
enum pmempool_check_result pmempool_check_end(PMEMpoolcheck *ppc);
```

##### Pool set synchronization and transformation: #####

```c
!ifdef{WIN32}
{
int pmempool_syncU(const char *poolset_file, unsigned flags, ...); (EXPERIMENTAL)
int pmempool_syncW(const wchar_t *poolset_file, unsigned flags, ...); (EXPERIMENTAL)
int pmempool_transformU(const char *poolset_file_src,
	const char *poolset_file_dst, unsigned flags, ...); (EXPERIMENTAL)
int pmempool_transformW(const wchar_t *poolset_file_src,
	const wchar_t *poolset_file_dst, unsigned flags, ...); (EXPERIMENTAL)
}{
int pmempool_sync(const char *poolset_file, unsigned flags, ...); (EXPERIMENTAL)
int pmempool_transform(const char *poolset_file_src,
	const char *poolset_file_dst, unsigned flags, ...); (EXPERIMENTAL)
}
```

##### Pool set management functions: #####

```c
!ifdef{WIN32}
{
int pmempool_rmU(const char *path, int flags);
int pmempool_rmW(const wchar_t *path, int flags);
}{
int pmempool_rm(const char *path, int flags);
}
```

##### Library API versioning: #####

```c
!ifdef{WIN32}
{
const char *pmempool_check_versionU(unsigned major_required,
	unsigned minor_required);
const wchar_t *pmempool_check_versionW(unsigned major_required,
	unsigned minor_required);
}{
const char *pmempool_check_version(unsigned major_required,
	unsigned minor_required);
}
```

##### Error handling: #####

```c
!ifdef{WIN32}
{
const char *pmempool_errormsgU(void);
const wchar_t *pmempool_errormsgW(void);
}{
const char *pmempool_errormsg(void);
}
```


# DESCRIPTION #

**libpmempool**
provides a set of utilities for off-line analysis and
manipulation of a *pool*. By *pool* in this
manpage we mean pmemobj pool, pmemblk pool, pmemlog pool or
BTT layout, independent of the underlying storage. Some of
**libpmempool** functions are required to work without
any impact on processed *pool* but some of them may
create a new or modify an existing one.

**libpmempool**
is for applications that need high reliability or built-in
troubleshooting. It may be useful for testing and debugging
purposes also.


# POOL CHECKING FUNCTIONS #

To perform check provided by **libpmempool**, a *check context*
must be first initialized using !pmempool_check_init
function described in this section. Once initialized
*check context* is represented by an opaque handle, of
type *PMEMpoolcheck\**, which is passed to all of the
other functions described in this section.

To execute check !pmempool_check must be called iteratively.
Each call resumes check till new status will be generated.
Each status is represented by !pmempool_check_status_ptr structure.
It may carry various
types of messages described in this section.

When check is completed !pmempool_check returns NULL pointer.
Check must be finalized using **pmempool_check_end**().
It returns *enum pmempool_check_result* describing
result of the whole check.

> NOTE: Currently, checking the consistency of a *pmemobj* pool is
**not** supported.

```c
!ifdef{WIN32}
{
PMEMpoolcheck *pmempool_check_initU(struct pmempool_check_argsU *args,
	size_t args_size);
PMEMpoolcheck *pmempool_check_initW(struct pmempool_check_argsW *args,
	size_t args_size);
}{
PMEMpoolcheck *pmempool_check_init(struct pmempool_check_args *args,
	size_t args_size);
}
```

The !pmempool_check_init initializes check
context. *args* describes parameters of the
check context. *args_size* should be equal to
the size of the !pmempool_check_args.
!pmempool_check_args is defined as follows:

!ifdef{WIN32}
{
```c
struct pmempool_check_argsU
{
	/* path to the pool to check */
	const char *path;

	/* optional backup path */
	const char *backup_path;

	/* type of the pool */
	enum pmempool_pool_type pool_type;

	/* parameters */
	int flags;
};

struct pmempool_check_argsW
{
	/* path to the pool to check */
	const wchar_t *path;

	/* optional backup path */
	const wchar_t *backup_path;

	/* type of the pool */
	enum pmempool_pool_type pool_type;

	/* parameters */
	int flags;
};
```
}{
```c
struct pmempool_check_args
{
	/* path to the pool to check */
	const char *path;

	/* optional backup path */
	const char *backup_path;

	/* type of the pool */
	enum pmempool_pool_type pool_type;

	/* parameters */
	int flags;
};
```
}
The *flags* argument accepts any combination of the following values (ORed):

+ **PMEMPOOL_CHECK_REPAIR** - perform repairs
+ **PMEMPOOL_CHECK_DRY_RUN** - emulate repairs, not supported on Device DAX
+ **PMEMPOOL_CHECK_ADVANCED** - perform hazardous repairs
+ **PMEMPOOL_CHECK_ALWAYS_YES** - do not ask before repairs
+ **PMEMPOOL_CHECK_VERBOSE** - generate info statuses
+ **PMEMPOOL_CHECK_FORMAT_STR** - generate string format statuses

If provided parameters are invalid or initialization process fails
!pmempool_check_init returns NULL and sets *errno*
appropriately. *pool_type* has to match type of the
*pool* being processed. You can turn on pool type
detection by setting *pool_type* to **PMEMPOOL_POOL_TYPE_DETECT**.
Pool type detection fail ends check.

*backup_path* argument can either be:

+ NULL. It indicates no backup will be performed.

+ a non existing file. It is valid only in case *path* is a single file
*pool*. It indicates a *backup_path* file will be created and backup will be
performed.

+ an existing *pool set* file of the same structure (the same number of parts
with exactly the same size) as the source *pool set*. It is valid only in case
*path* is a *pool set*. It indicates backup will be performed in a form
described by the *backup_path* *pool set*.

Backup is supported only if the source *pool set* has no defined replicas.

Pool sets with remote replicas are not supported neither as *path* nor as
*backup_path*.

This is an example of a *check context* initialization:

```c
struct pmempool_check_args!U args =
{
	.path = "/path/to/blk.pool",
	.backup_path = NULL,
	.pool_type = PMEMPOOL_POOL_TYPE_BLK,
	.flags = PMEMPOOL_CHECK_REPAIR | PMEMPOOL_CHECK_DRY_RUN |
		PMEMPOOL_CHECK_VERBOSE | PMEMPOOL_CHECK_FORMAT_STR
};
```

```c
PMEMpoolcheck *ppc = pmempool_check_init!U{}(&args, sizeof(args));
```

The check will process a *pool* of type **PMEMPOOL_POOL_TYPE_BLK**
located in the path */path/to/blk.pool*. Before check it will
not create a backup of the *pool* (*backup_path == NULL*).
If the check will find any issues it will try to
perform repair steps (**PMEMPOOL_CHECK_REPAIR**), but it
will not make any changes to the *pool*
(**PMEMPOOL_CHECK_DRY_RUN**) and it will not perform any
dangerous repair steps (no **PMEMPOOL_CHECK_ADVANCED**).
The check will ask before performing any repair steps (no
**PMEMPOOL_CHECK_ALWAYS_YES**). It will also generate
detailed information about the check (**PMEMPOOL_CHECK_VERBOSE**).
**PMEMPOOL_CHECK_FORMAT_STR** flag indicates string
format statuses (*struct pmempool_check_status*).
Currently it is the only supported status format so this flag is required.

!ifdef{WIN32}
{
```c
struct pmempool_check_statusU *pmempool_checkU(PMEMpoolcheck *ppc);
struct pmempool_check_statusW *pmempool_checkW(PMEMpoolcheck *ppc);
```
}{
```c
struct pmempool_check_status *pmempool_check(PMEMpoolcheck *ppc);
```
}

The !pmempool_check function starts or resumes the check
indicated by *ppc*. When next status will be generated
it pauses the check and returns a pointer to the
!pmempool_check_status structure:

!ifdef{WIN32}
{
```c
struct pmempool_check_statusU
{
	enum pmempool_check_msg_type type; /* type of the status */
	struct
	{
		const char *msg; /* status message string */
		const char *answer; /* answer to message if applicable */
	} str;
};

struct pmempool_check_statusW
{
	enum pmempool_check_msg_type type; /* type of the status */
	struct
	{
		const wchar_t *msg; /* status message string */
		const wchar_t *answer; /* answer to message if applicable */
	} str;
};
```
}{
```c
struct pmempool_check_status
{
	enum pmempool_check_msg_type type; /* type of the status */
	struct
	{
		const char *msg; /* status message string */
		const char *answer; /* answer to message if applicable */
	} str;
};
```
}

This structure can describe three types of statuses:

+ **PMEMPOOL_CHECK_MSG_TYPE_INFO** - detailed information about the check.
  Generated only if a **PMEMPOOL_CHECK_VERBOSE** flag was set.
+ **PMEMPOOL_CHECK_MSG_TYPE_ERROR** - encountered error
+ **PMEMPOOL_CHECK_MSG_TYPE_QUESTION** - question. Generated only if an
  **PMEMPOOL_CHECK_ALWAYS_YES** flag was not set. It requires *answer* to be
  set to "yes" or "no" before continuing.

After calling !pmempool_check again the previously provided
!pmempool_check_status_ptr pointer must be
considered invalid. When the check completes
!pmempool_check returns NULL pointer.

```c
enum pmempool_check_result pmempool_check_end(PMEMpoolcheck* ppc);
```

The **pmempool_check_end**() function finalizes the check and
releases all related resources. *ppc* is not a valid
pointer after calling **pmempool_check_end**(). It
returns *enum pmempool_check_result* summarizing result
of the finalized check. **pmempool_check_end**() can
return one of the following values:

+ **PMEMPOOL_CHECK_RESULT_CONSISTENT** - the *pool* is consistent
+ **PMEMPOOL_CHECK_RESULT_NOT_CONSISTENT** - the *pool* is not consistent
+ **PMEMPOOL_CHECK_RESULT_REPAIRED** - the *pool* has issues but all repair
  steps completed successfully
+ **PMEMPOOL_CHECK_RESULT_CANNOT_REPAIR** - the *pool* has issues which
  can not be repaired
+ **PMEMPOOL_CHECK_RESULT_ERROR** - the *pool* has errors or the check
  encountered issue


# POOL SET SYNCHRONIZATION AND TRANSFORMATION #

Currently, the following operations are allowed only for **pmemobj** pools (see
**libpmemobj**(3)).


### POOL SET SYNC ###

```c
!ifdef{WIN32}
{
int pmempool_syncU(const char *poolset_file, unsigned flags, ...); (EXPERIMENTAL)
int pmempool_syncW(const wchar_t *poolset_file, unsigned flags, ...); (EXPERIMENTAL)
}{
int pmempool_sync(const char *poolset_file, unsigned flags); (EXPERIMENTAL)
}
```

The !pmempool_sync function synchronizes data between replicas within
a pool set.

!pmempool_sync accepts two mandatory arguments:

* *poolset_file* - a path to a pool set file,

* *flags* - a combination of flags (ORed) which modify the way of
synchronization.

>NOTE: Only the pool set file used to create the pool should be used
for syncing the pool.

The following flags are available:

* **PMEMPOOL_DRY_RUN** - do not apply changes, only check for viability of
synchronization,

* **PMEMPOOL_PROGRESS** - report progress of the operation via a callback
function passed as an optional argument.

!pmempool_sync function checks if metadata of all replicas in a pool set
are consistent, i.e. all parts are healthy, and if any of them is not,
the corrupted or missing parts are recreated and filled with data from one of
the healthy replicas.

The function returns either 0 on success or -1 in case of error
with proper errno set accordingly.

If the flag **PMEMPOOL_PROGRESS** is set, !pmempool_sync accepts an optional
argument: a pointer to a callback function for reporting progress of the
operation.

The callback has to have the type *PMEM_progress_cb* of the form:

```c
typedef int (*PMEM_progress_cb)(const char* msg, size_t curr, size_t total);
```

where

* *msg* - a message or a title for the progress report,

* *curr* - the current progress value,

* *total* - the maximum progress value.

It is assumed that NULL value of the *msg* breaks the current progress report
(e.g. in case of an error).

>NOTE: The !pmempool_sync API is experimental and it may change in future
versions of the library.

### POOL SET TRANSFORM ###

```c
!ifdef{WIN32}
{
int pmempool_transformU(const char *poolset_file_src,
	const char *poolset_file_dst,
	unsigned flags, ...); (EXPERIMENTAL)
int pmempool_transformW(const wchar_t *poolset_file_src,
	const wchar_t *poolset_file_dst,
	unsigned flags, ...); (EXPERIMENTAL)
}{
int pmempool_transform(const char *poolset_file_src,
	const char *poolset_file_dst,
	unsigned flags, ...); (EXPERIMENTAL)
}
```

The !pmempool_transform function modifies internal structure of a pool set.
It supports the following operations:

* adding one or more replicas,

* removing one or more replicas,

* reordering of replicas.


!pmempool_transform accepts three mandatory arguments:

* *poolset_file_src* - a path to a pool set file which defines the source
pool set to be changed,

* *poolset_file_dst* - a path to a pool set file which defines the target
structure of the pool set,

* *flags* - a combination of flags (ORed) which modify the way of
transformation.

The following flags are available:

* **PMEMPOOL_DRY_RUN** - do not apply changes, only check for viability of
transformation.

* **PMEMPOOL_PROGRESS** - report progress of the operation via a callback
function passed as an optional argument.

When adding or deleting replicas, the two pool set files can differ only in the
definitions of replicas which are to be added or deleted. One cannot add and
remove replicas in the same step. Only one of these operations can be performed
at a time. Reordering replicas can be combined with any of them.
Also, to add a replica it is necessary for its effective size to match or exceed
the pool size. Otherwise the whole operation fails and no changes are applied.
Effective size of a replica is the sum of sizes of all its part files decreased
by 4096 bytes per each part file. The 4096 bytes of each part file is
utilized for storing internal metadata of the pool part files.

The function returns either 0 on success or -1 in case of error
with proper *errno* set accordingly.

If the flag **PMEMPOOL_PROGRESS** is set, !pmempool_transform accepts an optional
argument: a pointer to a callback function for reporting progress of the
 operation.

The callback has to have the type *PMEM_progress_cb* of the form:

```c
typedef int (*PMEM_progress_cb)(const char* msg, size_t curr, size_t total);
```

where

* *msg* - a message or a title for the progress report,

* *curr* - the current progress value,

* *total* - the maximum progress value.

It is assumed that NULL value of the *msg* breaks the current progress report
(e.g. in case of an error).

>NOTE: The !pmempool_transform API is experimental and it may change in future
versions of the library.


# POOL SET MANAGEMENT FUNCTIONS: #

### Removing pool ###

```c
!ifdef{WIN32}
{
int pmempool_rmU(const char *path, int flags);
int pmempool_rmW(const wchar_t *path, int flags);
}{
int pmempool_rm(const char *path, int flags);
}
```

The !pmempool_rm function removes pool pointed by *path*. The *path* can
point to either a regular file, device dax or pool set file. In case of pool
set file the !pmempool_rm will remove all part files from local replicas
using **unlink**(3) and all remote replicas (supported on Linux)
using **rpmem_remove**() function (see **librpmem**(3)),
before removing the pool set file itself.

The *flags* argument determines the behavior of !pmempool_rm function.
It is either 0 or the bitwise OR of one or more of the following flags:

+ **PMEMPOOL_RM_FORCE**
Ignore all errors when removing part files from local replicas or remote
replica.

+ **PMEMPOOL_RM_POOLSET_LOCAL**
Remove also local pool set file.

+ **PMEMPOOL_RM_POOLSET_REMOTE**
Remove also remote pool set file.


# CAVEATS #

**libpmempool** relies on the library destructor being called from the main
thread. For this reason, all functions that might trigger destruction (e.g.
**dlclose**()) should be called in the main thread. Otherwise some of the
resources associated with that thread might not be cleaned up properly.


# LIBRARY API VERSIONING #

This section describes how the library API is versioned, allowing
applications to work with an evolving API.

```c
!ifdef{WIN32}
{
const char *pmempool_check_versionU(
	unsigned major_required,
	unsigned minor_required);
const wchar_t *pmempool_check_versionW(
	unsigned major_required,
	unsigned minor_required);
}{
const char *pmempool_check_version(
	unsigned major_required,
	unsigned minor_required);
}
```

The !pmempool_check_version function is used to see if
the installed **libpmempool** supports the version of the
library API required by an application. The easiest way to
do this for the application is to supply the compile-time
version information, supplied by defines in **\<libpmempool.h\>**, like this:

```c
reason = pmempool_check_version!U{}(PMEMPOOL_MAJOR_VERSION,
                                PMEMPOOL_MINOR_VERSION);
if (reason != NULL) {
	/* version check failed, reason string tells you why */
}
```

Any mismatch in the major version number is considered a failure, but a
library with a newer minor version number will pass this
check since increasing minor versions imply backwards compatibility.

An application can also check specifically for the existence of an
interface by checking for the version where that interface
was introduced. These versions are documented in this man
page as follows: unless otherwise specified, all interfaces
described here are available in version 1.0 of the library.
Interfaces added after version 1.0 will contain the text
*introduced in version x.y* in the section of this manual
describing the feature.

When the version check performed by !pmempool_check_version
is successful, the return value is NULL. Otherwise the
return value is a static string describing the reason for
failing the version check. The string returned by
!pmempool_check_version must not be modified or freed.


# DEBUGGING AND ERROR HANDLING #

Two versions of libpmempool are typically available on a development
system. The normal version, accessed when a program is
linked using the **-lpmempool** option, is optimized for
performance. That version skips checks that impact
performance and exceptionally logs any trace information or
performs any run-time assertions. If an error is detected
during the call to **libpmempool** function, an
application may retrieve an error message describing the
reason of failure using the following function:

```c
!ifdef{WIN32}
{
const char *pmempool_errormsgU(void);
const wchar_t *pmempool_errormsgW(void);
}{
const char *pmempool_errormsg(void);
}
```

The !pmempool_errormsg function returns a pointer to a
static buffer containing the last error message logged for
current thread. The error message may include description of
the corresponding error code (if *errno* was set), as returned
by **strerror**(3). The error message buffer is
thread-local; errors encountered in one thread do not affect
its value in other threads. The buffer is never cleared by
any library function; its content is significant only when
the return value of the immediately preceding call to
**libpmempool** function indicated an error, or if *errno*
was set. The application must not modify or free the error
message string, but it may be modified by subsequent calls
to other library functions.

A second version of **libpmempool**, accessed when a program uses
the libraries under !ifdef{WIN32}{**/nvml/src/x64/Debug**}{**/usr/lib/nvml_debug**}, contains
run-time assertions and trace points. The typical way to
access the debug version is to set the environment variable
**LD_LIBRARY_PATH** to !ifdef{WIN32}{**/nvml/src/x64/Debug** or other location}
{**/usr/lib/nvml_debug** or **/usr/lib64/nvml_debug**} depending on where the debug
libraries are installed on the system.
The trace points in
the debug version of the library are enabled using the
environment variable **PMEMPOOL_LOG_LEVEL**, which can be
set to the following values:

+ **0** - This is the default level when **PMEMPOOL_LOG_LEVEL** is not set.
No log messages are emitted at this level.

+ **1** - Additional details on any errors detected are logged (in addition to
returning the *errno*-based errors as usual). The same information may be
retrieved using !pmempool_errormsg.

+ **2** - A trace of basic operations is logged.

+ **3** - This level enables a very verbose amount of function call tracing in
the library.

+ **4** - This level enables voluminous and fairly obscure tracing information
that is likely only useful to the libpmempool developers.

The environment variable **PMEMPOOL_LOG_FILE** specifies a file name
where all logging information should be written. If the last
character in the name is "-", the PID of the
current process will be appended to the file name when the
log file is created. If **PMEMPOOL_LOG_FILE** is not set,
the logging output goes to stderr.

Setting the environment variable **PMEMPOOL_LOG_FILE** has no effect
on the non-debug version of **libpmempool**.


# EXAMPLE #

The following example illustrates how the **libpmempool** API is used.
The program detects the type and checks consistency of given pool.
If there are any issues detected, the pool is automatically repaired.

```c
#include <stddef.h>
!ifdef{WIN32}{}
{#include <unistd.h>}
#include <stdlib.h>
#include <stdio.h>
#include <libpmempool.h>

#define PATH "./pmem-fs/myfile"
#define CHECK_FLAGS (PMEMPOOL_CHECK_FORMAT_STR|PMEMPOOL_CHECK_REPAIR|\
                     PMEMPOOL_CHECK_VERBOSE)

int
main(int argc, char *argv[])
{
	PMEMpoolcheck *ppc;
	struct pmempool_check_status!U *status;
	enum pmempool_check_result ret;

	/* arguments for check */
	struct pmempool_check_args!U args = {
		.path		= PATH,
		.backup_path	= NULL,
		.pool_type	= PMEMPOOL_POOL_TYPE_DETECT,
		.flags		= CHECK_FLAGS
	};

	/* initialize check context */
	if ((ppc = pmempool_check_init!U{}(&args, sizeof(args))) == NULL) {
		perror("pmempool_check_init!U");
		exit(EXIT_FAILURE);
	}

	/* perform check and repair, answer 'yes' for each question */
	while ((status = pmempool_check!U{}(ppc)) != NULL) {
		switch (status->type) {
		case PMEMPOOL_CHECK_MSG_TYPE_ERROR:
			printf("%s\n", status->str.msg);
			break;
		case PMEMPOOL_CHECK_MSG_TYPE_INFO:
			printf("%s\n", status->str.msg);
			break;
		case PMEMPOOL_CHECK_MSG_TYPE_QUESTION:
			printf("%s\n", status->str.msg);
			status->str.answer = "yes";
			break;
		default:
			pmempool_check_end(ppc);
			exit(EXIT_FAILURE);
		}
	}

	/* finalize the check and get the result */
	ret = pmempool_check_end(ppc);
	switch (ret) {
		case PMEMPOOL_CHECK_RESULT_CONSISTENT:
		case PMEMPOOL_CHECK_RESULT_REPAIRED:
			return 0;
		default:
			return 1;
	}
}
```

See <http://pmem.io/nvml/libpmempool> for more examples using the
**libpmempool** API.


# ACKNOWLEDGEMENTS #

**libpmempool** builds on the persistent memory programming model
recommended by the SNIA NVM Programming Technical Work Group:
<http://snia.org/nvmp>


# SEE ALSO #

**mmap**(2), **munmap**(2), **msync**(2), **strerror**(3),
**libpmemobj**(3), **libpmemblk**(3), **libpmemlog**(3), **libpmem**(3)
and **<http://pmem.io>**

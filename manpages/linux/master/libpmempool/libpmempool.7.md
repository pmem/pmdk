---
layout: manual
Content-Style: 'text/css'
title: LIBPMEMPOOL!7
collection: libpmempool
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

[comment]: <> (libpmempool.7 -- man page for libpmempool)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[LIBRARY API VERSIONING](#library-api-versioning-1)<br />
[DEBUGGING AND ERROR HANDLING](#debugging-and-error-handling)<br />
[EXAMPLE](#example)<br />
[ACKNOWLEDGEMENTS](#acknowledgements)<br />
[SEE ALSO](#see-also)<br />


# NAME #

**libpmempool** -- persistent memory pool management library


# SYNOPSIS #

```c
#include <libpmempool.h>
cc -std=gnu99 ... -lpmempool -lpmem
```



##### Health check functions: #####

```c
PMEMpoolcheck *pmempool_check_init(struct pmempool_check_args *args,
	size_t args_size);
struct pmempool_check_status *pmempool_check(PMEMpoolcheck *ppc);
enum pmempool_check_result pmempool_check_end(PMEMpoolcheck *ppc);
```

##### Pool set synchronization and transformation: #####

```c
int pmempool_sync(const char *poolset_file, unsigned flags); (EXPERIMENTAL)
int pmempool_transform(const char *poolset_file_src,
	const char *poolset_file_dst,
	unsigned flags); (EXPERIMENTAL)
```

##### Pool set management functions: #####

```c
int pmempool_rm(const char *path, int flags);
```

##### Library API versioning: #####

```c
const char *pmempool_check_version(unsigned major_required,
	unsigned minor_required);
```

##### Error handling: #####

```c
const char *pmempool_errormsg(void);
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

**libpmempool** introduces functionality of pool set health check,
synchronization, transformation and removal.


# CAVEATS #

**libpmempool** relies on the library destructor being called from the main
thread. For this reason, all functions that might trigger destruction (e.g.
**dlclose**(3)) should be called in the main thread. Otherwise some of the
resources associated with that thread might not be cleaned up properly.


# LIBRARY API VERSIONING #

This section describes how the library API is versioned, allowing
applications to work with an evolving API.

```c
const char *pmempool_check_version(
	unsigned major_required,
	unsigned minor_required);
```

The **pmempool_check_version**() function is used to see if
the installed **libpmempool** supports the version of the
library API required by an application. The easiest way to
do this for the application is to supply the compile-time
version information, supplied by defines in **\<libpmempool.h\>**, like this:

```c
reason = pmempool_check_version(PMEMPOOL_MAJOR_VERSION,
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

When the version check performed by **pmempool_check_version**()
is successful, the return value is NULL. Otherwise the
return value is a static string describing the reason for
failing the version check. The string returned by
**pmempool_check_version**() must not be modified or freed.


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
const char *pmempool_errormsg(void);
```

The **pmempool_errormsg**() function returns a pointer to a
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
the libraries under **/usr/lib/nvml_debug**, contains
run-time assertions and trace points. The typical way to
access the debug version is to set the environment variable
**LD_LIBRARY_PATH** to **/usr/lib/nvml_debug** or **/usr/lib64/nvml_debug** depending on where the debug
libraries are installed on the system.
The trace points in
the debug version of the library are enabled using the
environment variable **PMEMPOOL_LOG_LEVEL**, which can be
set to the following values:

+ **0** - This is the default level when **PMEMPOOL_LOG_LEVEL** is not set.
No log messages are emitted at this level.

+ **1** - Additional details on any errors detected are logged (in addition to
returning the *errno*-based errors as usual). The same information may be
retrieved using **pmempool_errormsg**().

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
#include <unistd.h>
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
	struct pmempool_check_status *status;
	enum pmempool_check_result ret;

	/* arguments for check */
	struct pmempool_check_args args = {
		.path		= PATH,
		.backup_path	= NULL,
		.pool_type	= PMEMPOOL_POOL_TYPE_DETECT,
		.flags		= CHECK_FLAGS
	};

	/* initialize check context */
	if ((ppc = pmempool_check_init(&args, sizeof(args))) == NULL) {
		perror("pmempool_check_init");
		exit(EXIT_FAILURE);
	}

	/* perform check and repair, answer 'yes' for each question */
	while ((status = pmempool_check(ppc)) != NULL) {
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

**dlclose**(3), **strerror**(3), **libpmemobj**(3),
**libpmemblk**(3), **libpmemlog**(3), **libpmem**(3)
and **<http://pmem.io>**

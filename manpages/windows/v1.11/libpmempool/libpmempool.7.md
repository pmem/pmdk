---
layout: manual
Content-Style: 'text/css'
title: LIBPMEMPOOL
collection: libpmempool
header: PMDK
date: pmempool API version 1.3
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2016-2018, Intel Corporation)

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

**libpmempool** - persistent memory pool management library

# SYNOPSIS #

```c
#include <libpmempool.h>
cc  ... -lpmempool -lpmem
```


>NOTE: The PMDK API supports UNICODE. If the **PMDK_UTF8_API** macro is
defined, basic API functions are expanded to the UTF-8 API with postfix *U*.
Otherwise they are expanded to the UNICODE API with postfix *W*.

##### Library API versioning: #####

```c
const char *pmempool_check_versionU(
	unsigned major_required,
	unsigned minor_required);
const wchar_t *pmempool_check_versionW(
	unsigned major_required,
	unsigned minor_required);
```

##### Error handling: #####

```c
const char *pmempool_errormsgU(void);
const wchar_t *pmempool_errormsgW(void);
```

##### Other library functions: #####

A description of other **libpmempool** functions can be found on the following
manual pages:

+ health check functions: **pmempool_check_init**(3)

+ pool set synchronization and transformation: **pmempool_sync**(3)

+ pool set management functions: **pmempool_rm**(3)

+ toggle or query pool set features: **pmempool_feature_query**(3)

# DESCRIPTION #

**libpmempool**
provides a set of utilities for off-line analysis and
manipulation of a *pool*. A *pool* in this
manpage means a pmemobj pool, pmemblk pool, pmemlog pool or
BTT layout, independent of the underlying storage. Some
**libpmempool** functions are required to work without
any impact on the *pool* but some may create a new or modify
an existing *pool*.

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

This section describes how the library API is versioned,
allowing applications to work with an evolving API.

The **pmempool_check_versionU**()/**pmempool_check_versionW**() function is used to see if
the installed **libpmempool** supports the version of the
library API required by an application. The easiest way to
do this for the application is to supply the compile-time
version information, supplied by defines in **\<libpmempool.h\>**, like this:

```c
reason = pmempool_check_versionU(PMEMPOOL_MAJOR_VERSION,
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

When the version check performed by **pmempool_check_versionU**()/**pmempool_check_versionW**()
is successful, the return value is NULL. Otherwise the
return value is a static string describing the reason for
failing the version check. The string returned by
**pmempool_check_versionU**()/**pmempool_check_versionW**() must not be modified or freed.

# DEBUGGING AND ERROR HANDLING #

If an error is detected during the call to a **libpmempool** function, the
application may retrieve an error message describing the reason for the failure
from **pmempool_errormsgU**()/**pmempool_errormsgW**(). This function returns a pointer to a static buffer
containing the last error message logged for the current thread. If *errno*
was set, the error message may include a description of the corresponding
error code as returned by **strerror**(3). The error message buffer is
thread-local; errors encountered in one thread do not affect its value in
other threads. The buffer is never cleared by any library function; its
content is significant only when the return value of the immediately preceding
call to a **libpmempool** function indicated an error, or if *errno* was set.
The application must not modify or free the error message string, but it may
be modified by subsequent calls to other library functions.

Two versions of **libpmempool** are typically available on a development
system. The normal version, accessed when a program is linked using the
**-lpmempool** option, is optimized for performance. That version skips checks
that impact performance and never logs any trace information or performs any
run-time assertions.

A second version of **libpmempool**, accessed when a program uses the libraries
under **/pmdk/src/x64/Debug**, contains run-time assertions and trace points. The
typical way to access the debug version is to set the environment variable
**LD_LIBRARY_PATH** to **/pmdk/src/x64/Debug**. Debugging output is
controlled using the following environment variables. These variables have
no effect on the non-debug version of the library.

+ **PMEMPOOL_LOG_LEVEL**

The value of **PMEMPOOL_LOG_LEVEL** enables trace points in the debug version
of the library, as follows:

+ **0** - This is the default level when **PMEMPOOL_LOG_LEVEL** is not set.
No log messages are emitted at this level.

+ **1** - Additional details on any errors detected are logged (in addition
to returning the *errno*-based errors as usual). The same information
may be retrieved using **pmempool_errormsgU**()/**pmempool_errormsgW**().

+ **2** - A trace of basic operations is logged.

+ **3** - Enables a very verbose amount of function call tracing in the library.

+ **4** - Enables voluminous and fairly obscure tracing
information that is likely only useful to the **libpmempool** developers.

Unless **PMEMPOOL_LOG_FILE** is set, debugging output is written to *stderr*.

+ **PMEMPOOL_LOG_FILE**

Specifies the name of a file where
all logging information should be written. If the last character in the name
is "-", the *PID* of the current process will be appended to the file name when
the log file is created. If **PMEMPOOL_LOG_FILE** is not set, output is
written to *stderr*.

# EXAMPLE #

The following example illustrates how the **libpmempool** API is used.
The program detects the type and checks consistency of given pool.
If there are any issues detected, the pool is automatically repaired.

```c
#include <stddef.h>
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
	struct pmempool_check_statusU *status;
	enum pmempool_check_result ret;

	/* arguments for check */
	struct pmempool_check_argsU args = {
		.path		= PATH,
		.backup_path	= NULL,
		.pool_type	= PMEMPOOL_POOL_TYPE_DETECT,
		.flags		= CHECK_FLAGS
	};

	/* initialize check context */
	if ((ppc = pmempool_check_initU(&args, sizeof(args))) == NULL) {
		perror("pmempool_check_initU");
		exit(EXIT_FAILURE);
	}

	/* perform check and repair, answer 'yes' for each question */
	while ((status = pmempool_checkU(ppc)) != NULL) {
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

See <https://pmem.io/pmdk/libpmempool> for more examples using the
**libpmempool** API.

# ACKNOWLEDGEMENTS #

**libpmempool** builds on the persistent memory programming model
recommended by the SNIA NVM Programming Technical Work Group:
<https://snia.org/nvmp>

# SEE ALSO #

**dlclose**(3), **pmempool_check_init**(3), **pmempool_feature_query**(3),
**pmempool_rm**(3), **pmempool_sync**(3), **strerror**(3), **libpmem**(7),
**libpmemblk**(7), **libpmemlog**(7), **libpmemobj**(7)**
and **<https://pmem.io>**

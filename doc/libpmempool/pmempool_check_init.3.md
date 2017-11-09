---
layout: manual
Content-Style: 'text/css'
title: PMEMPOOL_CHECK_INIT!3
collection: libpmempool
header: NVM Library
date: pmempool API version 1.1
...

[comment]: <> (Copyright 2017, Intel Corporation)

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

[comment]: <> (pmempool_check_init.3 -- man page for pmempool health check functions)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[EXAMPLE](#example)<br />
[NOTES](#notes)<br />
[SEE ALSO](#see-also)<br />


# NAME #

!pmempool_check_init, !pmempool_check,
**pmempool_check_end**() -- checks pmempool health


# SYNOPSIS #

```c
#include <libpmempool.h>

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

!ifdef{WIN32}
{
>NOTE: NVML API supports UNICODE. If **NVML_UTF8_API** macro is defined then
basic API functions are expanded to UTF-8 API with postfix *U*,
otherwise they are expanded to UNICODE API with postfix *W*.
}


# DESCRIPTION #

To perform check provided by **libpmempool**, a *check context*
must be first initialized using !pmempool_check_init
function described in this section. Once initialized
*check context* is represented by an opaque handle, of
type *PMEMpoolcheck\**, which is passed to all of the
other functions available in **libpmempool**

To execute check !pmempool_check must be called iteratively.
Each call resumes check till new status will be generated.
Each status is represented by !pmempool_check_status_ptr structure.
It may carry various types of messages described in this section.

When check is completed !pmempool_check returns NULL pointer.
Check must be finalized using **pmempool_check_end**().
It returns *enum pmempool_check_result* describing
result of the whole check.

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

*pool_type* has to match type of the
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
considered invalid.

The **pmempool_check_end**() function finalizes the check and
releases all related resources. *ppc* is not a valid
pointer after calling **pmempool_check_end**().


# RETURN VALUE #

The !pmempool_check_init returns NULL and sets *errno*
appropriately if provided parameters are invalid or initialization process fails,
otherwise it returns an opaque handle, of type *PMEMpoolcheck\**.

The !pmempool_check returns NULL pointer when the check completes or
returns a pointer to the !pmempool_check_status structure
described above when next status is generated.

The **pmempool_check_end**() function
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


# EXAMPLE #

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


# NOTES #

Currently, checking the consistency of a *pmemobj* pool is
**not** supported.


# SEE ALSO #

**libpmemlog**(7), **libpmemobj**(7) and **<http://pmem.io>**

---
layout: manual
Content-Style: 'text/css'
title: _MP(PMEMPOOL_CHECK_INIT, 3)
collection: libpmempool
header: PMDK
date: pmempool API version 1.3
...

[comment]: <> (Copyright 2017-2018, Intel Corporation)

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

_UW(pmempool_check_init), _UW(pmempool_check),
**pmempool_check_end**() - checks pmempool health

# SYNOPSIS #

```c
#include <libpmempool.h>

_UWFUNCR1UW(PMEMpoolcheck, *pmempool_check_init, struct pmempool_check_args,
*args,=q=
	size_t args_size=e=)
_UWFUNCRUW(struct pmempool_check_status, *pmempool_check, PMEMpoolcheck *ppc)
enum pmempool_check_result pmempool_check_end(PMEMpoolcheck *ppc);
```

_UNICODE()

# DESCRIPTION #

To perform the checks provided by **libpmempool**, a *check context*
must first be initialized using the _UW(pmempool_check_init)
function described in this section. Once initialized, the
*check context* is represented by an opaque handle of
type *PMEMpoolcheck\**, which is passed to all of the
other functions available in **libpmempool**

To execute checks, _UW(pmempool_check) must be called iteratively.
Each call generates a new check status, represented by a
_UWS(pmempool_check_status) structure. Status messages are described
later below.

When the checks are completed, _UW(pmempool_check) returns NULL. The check
must be finalized using **pmempool_check_end**(), which returns an
*enum pmempool_check_result* describing the results of the entire check.

_UW(pmempool_check_init) initializes the check context. *args* describes
parameters of the check context. *args_size* should be equal to the size of
the _UWS(pmempool_check_args). _UWS(pmempool_check_args) is defined as follows:

_WINUX(=q=
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
=e=,=q=
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
=e=)

The *flags* argument accepts any combination of the following values (ORed):

+ **PMEMPOOL_CHECK_REPAIR** - perform repairs

+ **PMEMPOOL_CHECK_DRY_RUN** - emulate repairs, not supported on Device DAX

+ **PMEMPOOL_CHECK_ADVANCED** - perform hazardous repairs

+ **PMEMPOOL_CHECK_ALWAYS_YES** - do not ask before repairs

+ **PMEMPOOL_CHECK_VERBOSE** - generate info statuses

+ **PMEMPOOL_CHECK_FORMAT_STR** - generate string format statuses

*pool_type* must match the type of the *pool* being processed. Pool type
detection may be enabled by setting *pool_type* to
**PMEMPOOL_POOL_TYPE_DETECT**. A pool type detection failure ends the check.

*backup_path* may be:

+ NULL. No backup will be performed.

+ a non-existent file: *backup_path* will be created and backup will be
performed. *path* must be a single file *pool*.

+ an existing *pool set* file: Backup will be performed as defined by the
*backup_path* pool set. *path* must be a pool set, and *backup_path* must have
the same structure (the same number of parts with exactly the same size) as the
*path* pool set.

Backup is supported only if the source *pool set* has no defined replicas.

Neither *path* nor *backup_path* may specify a pool set with remote replicas.

The _UW(pmempool_check) function starts or resumes the check indicated by *ppc*.
When the next status is generated, the check is paused and _UW(pmempool_check)
returns a pointer to the _UWS(pmempool_check_status) structure:

_WINUX(=q=
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
=e=,=q=
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
=e=)

This structure can describe three types of statuses:

+ **PMEMPOOL_CHECK_MSG_TYPE_INFO** - detailed information about the check.
  Generated only if a **PMEMPOOL_CHECK_VERBOSE** flag was set.

+ **PMEMPOOL_CHECK_MSG_TYPE_ERROR** - An error was encountered.

+ **PMEMPOOL_CHECK_MSG_TYPE_QUESTION** - question. Generated only if an
  **PMEMPOOL_CHECK_ALWAYS_YES** flag was not set. It requires *answer* to be
  set to "yes" or "no" before continuing.

After calling _UW(pmempool_check) again, the previously provided
_UWS(pmempool_check_status) pointer must be considered invalid.

The **pmempool_check_end**() function finalizes the check and releases all
related resources. *ppc* is invalid after calling **pmempool_check_end**().

# RETURN VALUE #

_UW(pmempool_check_init) returns an opaque handle of type *PMEMpoolcheck\**.
If the provided parameters are invalid or the initialization process fails,
_UW(pmempool_check_init) returns NULL and sets *errno* appropriately.

Each call to _UW(pmempool_check) returns a pointer to a
_UWS(pmempool_check_status) structure when a status is generated. When the
check completes, _UW(pmempool_check) returns NULL.

The **pmempool_check_end**() function returns an *enum pmempool_check_result*
summarizing the results of the finalized check. **pmempool_check_end**() can
return one of the following values:

+ **PMEMPOOL_CHECK_RESULT_CONSISTENT** - the *pool* is consistent

+ **PMEMPOOL_CHECK_RESULT_NOT_CONSISTENT** - the *pool* is not consistent

+ **PMEMPOOL_CHECK_RESULT_REPAIRED** - the *pool* has issues but all repair
  steps completed successfully

+ **PMEMPOOL_CHECK_RESULT_CANNOT_REPAIR** - the *pool* has issues which
  can not be repaired

+ **PMEMPOOL_CHECK_RESULT_ERROR** - the *pool* has errors or the check
  encountered an issue

+ **PMEMPOOL_CHECK_RESULT_SYNC_REQ** - the *pool* has single healthy replica.
  To fix remaining issues use **pmempool_sync**(3).

# EXAMPLE #

This is an example of a *check context* initialization:

```c
struct _U(pmempool_check_args) args =
{
	.path = "/path/to/blk.pool",
	.backup_path = NULL,
	.pool_type = PMEMPOOL_POOL_TYPE_BLK,
	.flags = PMEMPOOL_CHECK_REPAIR | PMEMPOOL_CHECK_DRY_RUN |
		PMEMPOOL_CHECK_VERBOSE | PMEMPOOL_CHECK_FORMAT_STR
};
```

```c
PMEMpoolcheck *ppc = _U(pmempool_check_init)(&args, sizeof(args));
```

The check will process a *pool* of type **PMEMPOOL_POOL_TYPE_BLK**
located in the path */path/to/blk.pool*. Before the check it will
not create a backup of the *pool* (*backup_path == NULL*).
If the check finds any issues it will try to
perform repair steps (**PMEMPOOL_CHECK_REPAIR**), but it
will not make any changes to the *pool*
(**PMEMPOOL_CHECK_DRY_RUN**) and it will not perform any
dangerous repair steps (no **PMEMPOOL_CHECK_ADVANCED**).
The check will ask before performing any repair steps (no
**PMEMPOOL_CHECK_ALWAYS_YES**). It will also generate
detailed information about the check (**PMEMPOOL_CHECK_VERBOSE**).
The **PMEMPOOL_CHECK_FORMAT_STR** flag indicates string
format statuses (*struct pmempool_check_status*).
Currently this is the only supported status format so this flag is required.

# NOTES #

Currently, checking the consistency of a *pmemobj* pool is
**not** supported.

# SEE ALSO #

**libpmemlog**(7), **libpmemobj**(7) and **<https://pmem.io>**

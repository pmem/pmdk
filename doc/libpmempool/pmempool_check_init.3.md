---
draft: false
slider_enable: true
description: ""
disclaimer: "The contents of this web site and the associated <a href=\"https://github.com/pmem\">GitHub repositories</a> are BSD-licensed open source."
aliases: ["pmempool_check_init.3.html"]
title: "libpmempool | PMDK"
header: "pmempool API version 1.3"
---

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2017-2022, Intel Corporation)

[comment]: <> (pmempool_check_init.3 -- man page for pmempool health check functions)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[EXAMPLE](#example)<br />
[NOTES](#notes)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmempool_check_init**(), **pmempool_check**(),
**pmempool_check_end**() - checks pmempool health

# SYNOPSIS #

```c
#include <libpmempool.h>

PMEMpoolcheck *pmempool_check_init(struct pmempool_check_args *args,
	size_t args_size);
struct pmempool_check_status *pmempool_check(PMEMpoolcheck *ppc);
enum pmempool_check_result pmempool_check_end(PMEMpoolcheck *ppc);
```

# DESCRIPTION #

To perform the checks provided by **libpmempool**, a *check context*
must first be initialized using the **pmempool_check_init**()
function described in this section. Once initialized, the
*check context* is represented by an opaque handle of
type *PMEMpoolcheck\**, which is passed to all of the
other functions available in **libpmempool**

To execute checks, **pmempool_check**() must be called iteratively.
Each call generates a new check status, represented by a
*struct pmempool_check_status* structure. Status messages are described
later below.

When the checks are completed, **pmempool_check**() returns NULL. The check
must be finalized using **pmempool_check_end**(), which returns an
*enum pmempool_check_result* describing the results of the entire check.

**pmempool_check_init**() initializes the check context. *args* describes
parameters of the check context. *args_size* should be equal to the size of
the *struct pmempool_check_args*. *struct pmempool_check_args* is defined as follows:

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

The **pmempool_check**() function starts or resumes the check indicated by *ppc*.
When the next status is generated, the check is paused and **pmempool_check**()
returns a pointer to the *struct pmempool_check_status* structure:

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

This structure can describe three types of statuses:

+ **PMEMPOOL_CHECK_MSG_TYPE_INFO** - detailed information about the check.
  Generated only if a **PMEMPOOL_CHECK_VERBOSE** flag was set.

+ **PMEMPOOL_CHECK_MSG_TYPE_ERROR** - An error was encountered.

+ **PMEMPOOL_CHECK_MSG_TYPE_QUESTION** - question. Generated only if an
  **PMEMPOOL_CHECK_ALWAYS_YES** flag was not set. It requires *answer* to be
  set to "yes" or "no" before continuing.

After calling **pmempool_check**() again, the previously provided
*struct pmempool_check_status* pointer must be considered invalid.

The **pmempool_check_end**() function finalizes the check and releases all
related resources. *ppc* is invalid after calling **pmempool_check_end**().

# RETURN VALUE #

**pmempool_check_init**() returns an opaque handle of type *PMEMpoolcheck\**.
If the provided parameters are invalid or the initialization process fails,
**pmempool_check_init**() returns NULL and sets *errno* appropriately.

Each call to **pmempool_check**() returns a pointer to a
*struct pmempool_check_status* structure when a status is generated. When the
check completes, **pmempool_check**() returns NULL.

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
struct pmempool_check_args args =
{
	.path = "/path/to/obj.pool",
	.backup_path = NULL,
	.pool_type = PMEMPOOL_POOL_TYPE_OBJ,
	.flags = PMEMPOOL_CHECK_VERBOSE | PMEMPOOL_CHECK_FORMAT_STR
};
```

```c
PMEMpoolcheck *ppc = pmempool_check_init(&args, sizeof(args));
```

The check will process a *pool* of type **PMEMPOOL_POOL_TYPE_OBJ**
located in the path */path/to/obj.pool*. Before the check it will
not create a backup of the *pool* (*backup_path == NULL*).
It will also generate
detailed information about the check (**PMEMPOOL_CHECK_VERBOSE**).
The **PMEMPOOL_CHECK_FORMAT_STR** flag indicates string
format statuses (*struct pmempool_check_status*).
Currently this is the only supported status format so this flag is required.

# NOTES #

Currently, checking the consistency of a *pmemobj* pool is
**not** supported.

# SEE ALSO #

**libpmemobj**(7) and **<https://pmem.io>**

---
layout: manual
Content-Style: 'text/css'
title: _MP(MINIASYNC_VDM, 7)
collection: miniasync
header: MINIASYNC_VDM
secondary_title: miniasync
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2022, Intel Corporation)

[comment]: <> (miniasync_vdm.7 -- man page for miniasync vdm API)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**miniasync_vdm** - virtual data mover API for miniasync library

# SYNOPSIS #

```c
#include <libminiasync.h>

typedef void *(*vdm_operation_new)
	(struct vdm *vdm, const enum vdm_operation_type type);
typedef int (*vdm_operation_start)(void *data,
	const struct vdm_operation *operation,
	struct future_notifier *n);
typedef enum future_state (*vdm_operation_check)(void *data,
	const struct vdm_operation *operation);
typedef void (*vdm_operation_delete)(void *data,
	const struct vdm_operation *operation,
	struct vdm_operation_output *output);

struct vdm {
	vdm_operation_new op_new;
	vdm_operation_delete op_delete;
	vdm_operation_start op_start;
	vdm_operation_check op_check;
};

enum vdm_operation_type {
	VDM_OPERATION_MEMCPY,
	VDM_OPERATION_MEMMOVE,
	VDM_OPERATION_MEMSET,
};

enum vdm_operation_result {
	VDM_SUCCESS,
	VDM_ERROR_OUT_OF_MEMORY,
	VDM_ERROR_JOB_CORRUPTED,
};

struct vdm_operation_data {
	void *op;
	struct vdm *vdm;
};

struct vdm_operation_output {
	enum vdm_operation_type type;
	enum vdm_operation_result result;
	union {
		struct vdm_operation_output_memcpy memcpy;
		struct vdm_operation_output_memmove memmove;
	} output;
};
```

For general description of miniasync, see **miniasync**(7).

# DESCRIPTION #

Virtual data mover API forms the basis of various concrete data movers.
It is an abstraction that the data mover implementations should adapt for
compatibility with the **miniasync_future**(7) feature.

*struct vdm* is a structure required by every virtual data mover operation, for
example **vdm_memcpy**(3). *struct vdm* has following members:

* *op_new* - allocations needed for the specified operation type

* *op_delete* - deallocations and finalizations of operation

* *op_start* - populates operation data and starts the execution of the task

* *op_check* - data mover task status check

Currently, virtual data mover API supports following operation types:

* **VDM_OPERATION_MEMCPY** - a memory copy operation
* **VDM_OPERATION_MEMMOVE** - a memory move operation
* **VDM_OPERATION_MEMSET** - a memory set operation

For more information about concrete data mover implementations, see **miniasync_vdm_threads**(7),
**miniasync_vdm_synchronous**(7) and **miniasync_vdm_dml**(7).

For more information about the usage of virtual data mover API, see *examples* directory
in miniasync repository <https://github.com/pmem/miniasync>.

## RETURN VALUE ##

The vdm operations always return a new instance of a `vdm_operation_future`
structure. If initialization is successful, this future is idle and ready to be
polled. Otherwise, the future is immediately complete and the specific error
can be read from the *result* field of the *vdm_operation_output* structure.
Already complete can also be safely polled, which has no effect.
Depending on the vdm implementation, the operation's processing can also fail,
in which case the error can be similarly found in the same location.

The *result* field can be set to one of the following values:

* **VDM_ERROR_OUT_OF_MEMORY** - data mover has insufficient memory to create a
	a new operation. This usually indicates that there are too many pending
	operations.
* **VDM_ERROR_JOB_CORRUPTED** - data mover encountered an error during internal
	job processing. The specific cause depends on the implementation.

# SEE ALSO #

**vdm_memcpy**(3), **vdm_memmove**(3), **vdm_memset**(3),
**miniasync**(7), **miniasync_future**(7),
**miniasync_vdm_dml**(7), **miniasync_vdm_synchronous**(7),
**miniasync_vdm_threads**(7) and **<https://pmem.io>**

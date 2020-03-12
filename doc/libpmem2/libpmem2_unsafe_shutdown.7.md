---
layout: manual
Content-Style: 'text/css'
title: _MP(LIBPMEM2, 7)
collection: libpmem2
header: PMDK
date: pmem2 API version 1.0
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause
[comment]: <> (Copyright 2019-2020, Intel Corporation)

[comment]: <> (libpmem2_unsafe_shutdown.7 -- man page for libpmem2 unsafe shutdown)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[GRANULARITY](#unsafe_shutdown)<br />
[CAVEATS](#unsafe_shutdown_handling)<br />
[EXAMPLE](#example)<br />
[SEE ALSO](#see-also)

# NAME #

**libpmem2_unsafe_shutdown** - libpmem2 unsafe shutdown

# SYNOPSIS #

```c
#include <libpmem2.h>
```

# DESCRIPTION #

In systems with persistent memory support a *power-fail protected domain*
covers a set of resources from which data will be flushed to *persistent medium*
in case of *power-failure*. Data stored on *persistent medium* is preserved
across power cycles.

The capability to flush all data stored in *power-fail protected domain* to
*persistent medium* is hardware-guaranteed. Because this capability is
mission-critical the persistent memory support also requires the possibility
to detect cases when hardware fails to fulfill its guarantees in the face of
power failure. Such an event is called *unsafe (dirty) shutdown*. In case of
*unsafe shutdown*, data, which have not been flushed to *persistent medium*,
is lost.

To mitigate this risk NVDIMMs exposes *unsafe shutdown counter* (**USC**).
**USC** value is stored on each NVDIMM and its value is bumped up each time
NVDIMM were attached to the system during *unsafe shutdown*.

# UNSAFE SHUTDOWN DETECTION #

To detect if *unsafe shutdown* has taken place and it might affect the memory
pool you have to consider few factors:

- were your memory pool closed unexpectedly? If you have closed the memory pool
cleanly your data for sure was not affected.
- has **USC** value changed? you have to check all NVDIMMs backing the memory
pool (*NVDIMMs set*).
- is your memory pool still use the same *NVDIMMs set*? It is possible
the memory pool was copied to a different *NVDIMMs set*. You have to make sure
you are comparing **USC** value obtained from the same *NVDIMMs set*.
In case of detecting the memory pool *NVDIMMs set* has altered you have to
invalidate all knowledge you have collected about the previously used *NVDIMM set*
e.g. previous **USC** value. To obtain a unique identifier of NVDIMMs set
you can use **pmem2_source_device_id**(3).

Considering all of these a general sequence of checks, which have to be performed
each time the pool is going to be used, looks as follow:

```c
if (!unsafe_shutdown_info_initialized) {
	unsafe_shutdown_info_initialize();
	return;
}

if ((old_usc == new_usc) && (old_device_id == new_device_id)) {

	/* the unsafe shutdown has NOT occurred... */

	if (!pool_in_use) {
		/* ... and the pool has been closed correctly. */
		return;
	}

	/* ... but the pool has not been closed correctly. */
	unsafe_shutdown_info_initialize(); /* reinitialize */
	return;
} else {

	/* the unsafe shutdown HAS occurred... */

	if (!pool_in_use) {
		/* ... but the pool was not in use during that event. */
		unsafe_shutdown_info_initialize(); /* reinitialize */
		return;
	}

	/* ... and the pool was in use. The pool might be corrupted. */
}
```

# REQUIRED VALUES #

To implement such *unsafe shutdown* detection you have to store in your pool
three values:

- ```old_usc``` obtained using **pmem2_source_device_usc**(3),
- ```old_device_id``` obtained using **pmem2_source_device_id**(3)
(or its hash) and
- ```pool_in_use``` which is an indicator whether the pool is currently in use.
It has to indicate *NOT_IN_USE* if the pool is not in use. Before performing
any other operation, you have to alter its value to *IN_USE*. Similarly
when you have done everything and you will be closing the pool you have to set
```pool_in_use``` value to *NOT_IN_USE*. (exact *NOT_IN_USE* and *IN_USE* values
are not important).

**Note**: The *unsafe shutdown* detection should be the first thing you do after
reopening the memory pool. So you have to do this before setting ```pool_in_use```
variable to *IN_USE*.

# REQUIRED PROCEDURES #

To store the before mentioned values you have to use **pmem2_deep_sync**(3).
The **pmem2_deep_sync**(3) function guarantees your values reach
*persistent medium* before it will return. This way your values will be persistent
in case of **unsafe shutdown** so you can rely on them.

You also have to consider ensuring consistency of this data e.g. using a checksum.

```c
/* XXX any required alignment? */
struct shutdown_info {
	uint64_t old_usc;
	uint64_t old_device_id;
	uint64_t pool_in_use;
	uint64_t checksum;
} *info;

struct pmem2_source *src;
struct pmem2_map *map;
pmem2_persist_fn persist;

/* invalidate old values */
info->checksum = 0;
persist(info, sizeof(*info));

/* store new values */
pmem2_source_device_usc(src, &info->old_usc);

char device_id[MAX_LEN];
size_t len = MAX_LEN;
pmem2_source_device_id(src, device_id, &len);
info->old_device_id = custom_checksum((void *)device_id, len);

info->pool_in_use = NOT_IN_USE;

/* persist and deep sync the new values */
persist(info, sizeof(*info));
pmem2_deep_sync(map, info, sizeof(*info));

/* validate the new values */
info->checksum = custom_checksum((void *)info, INFO_SIZE_WITHOUT_CHKSUM);
persist(info, sizeof(*info));
pmem2_deep_sync(map, info, sizeof(*info));
```

# SEE ALSO #

**pmem2_deep_sync**(3), **pmem2_source_device_id**(3), **pmem2_source_device_usc**(3)
and **<https://pmem.io>**

---
layout: manual
Content-Style: 'text/css'
title: _MP(LIBPMEM2_UNSAFE_SHUTDOWN, 7)
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
[GRANULARITY](#unsafe-shutdown-detection)<br />
[CAVEATS](#required-values)<br />
[CAVEATS](#required-procedures)<br />
[SEE ALSO](#see-also)

# NAME #

**libpmem2_unsafe_shutdown** - libpmem2 unsafe shutdown

# SYNOPSIS #

```c
#include <libpmem2.h>
```

# DESCRIPTION #

In systems with the persistent memory support, *a power-fail protected domain*
covers a set of resources from which data will be flushed to *a persistent medium*
in case of *a power-failure*. Data stored on *the persistent medium* is preserved
across power cycles.

The feature to flush all data stored in *the power-fail protected domain* to
*the persistent medium* is hardware-guaranteed. Since this feature is
mission-critical, the persistent memory support also requires the possibility
to detect cases when hardware fails to fulfill its guarantees in the face of
a power failure. Such an event is called *an unsafe (dirty) shutdown*. In case of
*the unsafe shutdown*, data that has not been flushed to *the persistent medium*,
is lost.

To mitigate this risk NVDIMMs expose *an unsafe shutdown counter* (**USC**).
**USC** value is stored on each NVDIMM and its value is bumped up each time,
the NVDIMMs are attached to the system during *the unsafe shutdown*.

# REQUIRED VALUES #

To implement *unsafe shutdown* detection, you have to store in the file
three values:

- ```old_usc``` obtained using **pmem2_source_device_usc**(3),
- ```old_device_id``` obtained using **pmem2_source_device_id**(3)
(or its hash) and
- ```file_in_use``` which is an indicator whether the file is currently in use.
Its value should be *FALSE* if the file is not in use. Before performing
data modification, you have to alter its value to *TRUE*. Similarly,
when you are done modifying and you will be closing the file you have to set
```file_in_use``` value to *FALSE*.

> **Note**: The *unsafe shutdown* detection should be the first thing you do after
> reopening the memory pool. This means you have to do this before setting
> ```pool_in_use``` variable to *TRUE*.

# UNSAFE SHUTDOWN DETECTION #

To detect if *the unsafe shutdown* has taken place and it might have affected
the file, you have to consider a few factors:

- was the file closed unexpectedly? If the file has been closed
cleanly, it's certain your data hasn't been corrupted.
- has the **USC** value changed? All NVDIMMs backing the file (*NVDIMMs set*)
have to be checked.
- does the file still is on the same *NVDIMMs set*? It is possible
the file has been copied to a different *NVDIMMs set*. You have to make sure
you are comparing **USC** value obtained from the same *NVDIMMs set*.
In case it was detected the file's *NVDIMMs set* has been altered, you have to
invalidate all knowledge you have collected about the previously used *NVDIMM set*
e.g. previous **USC** value. To obtain a unique identifier of *NVDIMMs set*,
you can use **pmem2_source_device_id**(3).

Consider all of this a general sequence of checks, which has to be performed
each time the file is going to be used looks as follow:

```c
if (!unsafe_shutdown_info_initialized) {
	unsafe_shutdown_info_initialize();
} else if (old_device_id == new_device_id) {
	if (old_usc == new_usc) {
		/* the unsafe shutdown has NOT occurred. */
	} else {

		/* the unsafe shutdown HAS occurred... */

		if (file_in_use) {
			/* ... and the file was in use. The file might be corrupted. */
		} else {
			/* ... but the file was not in use. Data is safe. */
			unsafe_shutdown_info_initialize(); /* reinitialize */
		}
	}
} else {
	/* the file has been moved, the USC value doesn't matter. */
	unsafe_shutdown_info_initialize(); /* reinitialize */
}
```

# REQUIRED PROCEDURES #

To store the aforementioned values you have to use **pmem2_deep_sync**(3).
The **pmem2_deep_sync**(3) function guarantees your values will reach
*the persistent medium* before it will return. This way your values will be
persistent in case of *the unsafe shutdown* so you can rely on them.

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

**pmem2_deep_sync**(3), **pmem2_persist_fn**(3), **pmem2_source_device_id**(3),
**pmem2_source_device_usc**(3) and **<https://pmem.io>**

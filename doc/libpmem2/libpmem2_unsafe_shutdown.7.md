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
[DESCRIPTION](#description)<br />
[UNSAFE SHUTDOWN DETECTION](#unsafe-shutdown-detection)<br />
[DEEP SYNC REQUIRE](#deep-sync-required)<br />
[CHECKSUM](#checksum)<br />
[SEE ALSO](#see-also)

# NAME #

**libpmem2_unsafe_shutdown** - libpmem2 unsafe shutdown

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
**USC** value is stored on each NVDIMM and its value is increased each time
*the unsafe shutdown* happens. **USC** is a monotonically increasing counter.

> **Note**: *The unsafe shutdown* may corrupt data stored on a device, in a file,
> in a set of files and in a mapping spanning only a part of a file.
> For the sake of simplicity, all of these cases in this document will be called
> *the file*.

# UNSAFE SHUTDOWN DETECTION #

A basic way of detecting *the unsafe shutdown* is by noticing the **USC** value
change. It requires comparing the past value of **USC** (```old_usc```)
with the current **USC** value (```new_usc```). If ```new_usc > old_usc``` it is
assumed *the unsafe shutdown* occurred.

A current **USC** value can be obtained using **pmem2_source_device_usc**(3).
The ```old_usc``` value has to be stored by the user on a medium which will
preserve it across power cycles e.g. *the persistent medium*.

> **Note**: Storing the ```old_usc``` value on *the persistent medium* despite
> being a natural choice requires considering a few caveats. For details please
> see the [DEEP SYNC REQUIRE](#deep-sync-required) section.

The **USC**-only approach produces many false-positives. It should be applied
only if rebuilding the data, in a result of false-detected *unsafe shutdown*,
is not very expensive.

In the basic *unsafe shutdown* detection method described above a one way of
inducing **USC** mismatch (and at the same time false-detecting *unsafe shutdown*)
is by copying *the file*. If *the file* have been copied to a different
*NVDIMMs set* all the knowledge which have been collected have to be invalidated
e.g. ```old_usc```. A unique identifier of the *NVDIMMs set* may be obtained
using **pmem2_source_device_id**(3). It also has to be stored by the
user on a medium which will preserve it across power cycles (```old_device_id```).

The following pseudo-code takes into account ```old_usc``` and ```old_device_id```:

```c
if (!unsafe_shutdown_info_initialized) {
	unsafe_shutdown_info_initialize();
} else if (old_device_id == new_device_id) {
	if (old_usc == new_usc) {
		/* the unsafe shutdown has NOT occurred. */
	} else {
		/* the unsafe shutdown HAS occurred. The file might be currupted. */
	}
} else {
	/* the file has been moved, the USC value doesn't matter. */
	unsafe_shutdown_info_initialize(); /* reinitialize */
}
```

*The unsafe shutdown* cannot corrupt data already stored on *the persistent medium*.
If *the file* has been closed cleanly, it's certain the data hasn't been corrupted.
To take into account this factor, an indicator of *the file* being in use
(```file_in_use```) has to be added. Its value should be *FALSE* if the file is
not in use. Before performing any data modification, its value has to be altered
to *TRUE*. Similarly, when all modifications are done and *the file* will be
closed the ```file_in_use``` value has to be set to *FALSE*.

> **Note**: The *unsafe shutdown* detection should be the first thing done after
> reopening *the file*. This means it has to be done before setting
> ```pool_in_use``` variable to *TRUE*.

A robust (and fine-grained) *unsafe shutdown* detection which takes into account
all of the above factors looks as follow:

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
	/*  the USC value doesn't matter. The file was moved... */

	if (file_in_use) {
		/* ... and was closed NOT cleanly. The file might be corrupted. */
	} else {
		/* ... but after being closed cleanly. Data is safe. */
		unsafe_shutdown_info_initialize(); /* reinitialize */
	}
}
```

# DEEP SYNC REQUIRED #

A *deep sync* is a way of making sure all the data already persisted will be
preserved in the face of *the unsafe shutdown*. **pmem2_deep_sync**() allows
proceeding with a program execution after making sure data has reached
*the persistent medium*. So it is a required element in implementing any
algorithm detecting *the unsafe shutdown*. For details how to use
**pmem2_deep_sync**() please see the **pmem2_deep_sync**(3) manual page.

An application which implements the basic way of detecting *the unsafe shutdown*
has to store the ```old_usc``` value on *the persistent medium*
(*the power-fail protected domain* is not enough since it cannot rely on it
implementing an algorithm of detecting its failure). To achieve this the
```old_usc``` value has to be *deep synced* e.g.:

```c
struct shutdown_info_basic {
	uint64_t old_usc;
} *info;

struct pmem2_source *src;
struct pmem2_map *map;
pmem2_persist_fn persist;

/* store a USC value */
pmem2_source_device_usc(src, &info->old_usc);

/* persist and deep sync the new value */
persist(info, sizeof(*info));
pmem2_deep_sync(map, info, sizeof(*info));

/* old_usc is on the persistent medium so unsafe shutdown can't corrupt it. */
```

Similarily ```old_device_id``` has to be *deep synced*. Things are a little bit
more complex regarding the ```file_in_use``` variable. It has to be *deep synced*
after setting its value to *TRUE* and before proceeding with the application but
also *the file* has to be *deep synced* before changing ```file_in_use``` value
to *FALSE*. A pseudo-code showing how to deal with it looks as follow:

```c
uint64_t *file_in_use;
void *mapping_address;
size_t mapping_size;

/* after opening the file*/
assert(*file_in_use == FALSE);
*file_in_use = TRUE;
persist(file_in_use, sizeof(*file_in_use));
pmem2_deep_sync(map, file_in_use, sizeof(*file_in_use));

/* the file is opened. It is safe to modify. */

/* deep sync all changes before closing the file */
pmem2_deep_sync(map, mapping_address, mapping_size);

/* closing the file */
*file_in_use = FALSE;
persist(file_in_use, sizeof(*file_in_use));
pmem2_deep_sync(map, file_in_use, sizeof(*file_in_use));

/* the file is closed cleanly. */
```

# CHECKSUM #

Aforementioned logic requires storing multiple variables. To make reasonable use
of them they must be consistent with each other e.g. if *the unsafe shutdown*
happens between storing ```old_usc``` and ```old_device_id``` their values are
inconsistent so after reboot the, the algorithm will detect *the unsafe shutdown*
where it hasn't happen or, even worse, not detect *the unsafe shutdown* even it
has happened.

Storing a checksum along with the all required variables gives a certainty that
the read data is consistent. The pseudo-code using this method looks as follow:

```c
struct shutdown_info {
	uint64_t old_usc;
	uint64_t old_device_id;
	uint64_t file_in_use;
	uint64_t checksum;
} *info;

/* store new values */
/* ... */

/* validate the new values */
info->checksum = custom_checksum((void *)info,
	sizeof(*info) - sizeof(info->checksum));

/* persist and deep sync the whole structure */
persist(info, sizeof(*info));
pmem2_deep_sync(map, info, sizeof(*info));
```

> **Note**: Storing checksum along with other variables requires only a single
> persist and one *deep sync* for the whole structure. After the *deep sync* the
> data and checksum are stored on the *persistent medium*. If *unsafe shutdown*
> happens at any time during this process the user will detect a data-checksum
> mismatch. But detecting the data-checksum mismatch, despite it means
> *the unsafe shutdown* took place, in this case it does not mean a *usable data*
> (other than ```struct shutdown_info```) is corrupted. Since the checksum is
> modified before modifying any *usable data* or after *deep-syncing* all changes
> made to the *the usable data*, the data-checksum mismatch means the data was
> safe during *the unsafe shutdown*.

# SEE ALSO #

**pmem2_deep_sync**(3), **pmem2_persist_fn**(3), **pmem2_source_device_id**(3),
**pmem2_source_device_usc**(3) and **<https://pmem.io>**

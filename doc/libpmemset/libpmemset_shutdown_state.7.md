---
layout: manual
Content-Style: 'text/css'
title: _MP(LIBPMEMSET_SHUTDOWN_STATE, 7)
collection: libpmemset
header: PMDK
date: pmemset API version 1.0
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2021, Intel Corporation)

[comment]: <> (libpmemset_shutdown_state.7 -- man page for libpmemset shutdown state)

[NAME](#name)<br />
[DESCRIPTION](#description)<br />
[SHUTDOWN STATE](#shutdown-state)<br />
[EXAMPLE](#example)<br />
[SEE ALSO](#see-also)

# NAME #

**libpmemset_shutdown_state** - libpmemset shutdown state

# DESCRIPTION #

In systems with the persistent memory support, *a power-fail protected domain*
covers a set of resources from which the platform will flush data to the
*a persistent medium* in case of *a power-failure*. Data stored on
*the persistent medium* is preserved across power cycles.

The hardware guarantees the feature to flush all data stored in
*the power-fail protected domain* to *the persistent medium*. However, nothing
is infallible, and Persistent Memory hardware can expose a monotonically
increasing *unsafe shutdown counter* (**USC**) that is incremented every time
a failure of the mechanism above is detected. This allows software to discover
situations where a running application was interrupted by a power failure that
led to an unsafe shutdown. Undiscovered unsafe shutdowns might cause silent data
corruption.

>Note: Unsafe shutdown count is supported only on the hardware with
SMART (Self-Monitoring, Analysis and Reporting Technology) monitoring system included.

# SHUTDOWN STATE #

Shutdown state (SDS) feature relies on the retrieval of unsafe shutdown count from the
underlying hardware. It is done using **libpmem2**(7) library, for more information
about concepts related to the *unsafe shutdown* please see **libpmem2_unsafe_shutdown**(7).

SDS evaluation is based on a comparison of SDS values set in the source and SDS information
freshly retrieved during the mapping process. It is done for each new mapping created using
**pmemset_map**(3) from a source *struct pmemset_source* that has an SDS *struct pmemset_sds*
structure bound to it using **pmemset_source_set_sds**(3) function.

Optionally, along with the SDS structure, a pointer to the part state *enum pmemset_part_state*
structure can be provided to the source. After the SDS evaluation of a newly mapped part, its
evaluated state is returned via the provided pointer.

A newly mapped part can have one of the following states:

* **PMEMSET_PART_STATE_INDETERMINATE** - the pool state cannot be determined because of
the errors during retrieval of device information.

* **PMEMSET_PART_STATE_OK** - the pool is internally consistent and was closed cleanly.
Application can assume that no custom recovery is needed.

* **PMEMSET_PART_STATE_OK_BUT_ALREADY_OPEN** - the pool is internally consistent, but it
was not closed cleanly. Application must perform consistency checking and custom recovery
on user data. It is also an expected state when creating multiple mapping from the same
*pmemset_source*.

* **PMEMSET_PART_STATE_CORRUPTED** - the pool can contain invalid data as a result of hardware
failure. Reading the pool is unsafe.

When a part state is evaluated as anything but **PMEMSET_PART_STATE_OK**, by default
it will cause an error and terminate the mapping process. It can be changed, user
can provide a list of acceptable part states that will not produce an error when
evaluated. For more information please see **pmemset_config_set_acceptable_states**(3).

SDS values can change during the usage of **libpmemset**(7) library, each time an SDS
is updated a *PMEMSET_EVENT_SDS_UPDATE* event is triggered. Updated SDS can be
obtained via callback bound to this event. For more information about events and
its callbacks please see **pmemset_config_set_event_callback**(3).

# EXAMPLE #

Example usage of the SDS feature:

```c
struct pmemset *set;
struct pmemset_config *cfg;
struct pmemset_map_config *map_cfg;
struct pmemset_sds sds = PMEMSET_SDS_INITIALIZE();
struct pmemset_source *src;
const char *file = "somefile";
enum pmemset_part_state state;
uint64_t acceptable_states;

pmemset_config_new(&cfg);
pmemset_config_set_required_store_granularity(cfg, PMEM2_GRANULARITY_PAGE);

acceptable_states = (PMEMSET_PART_STATE_OK |
			PMEMSET_PART_STATE_OK_BUT_ALREADY_OPEN);
pmemset_config_set_acceptable_states(cfg, acceptable_states);

pmemset_new(&set, cfg);

pmemset_map_config_new(&map_cfg, set);

pmemset_source_from_file(&src, file);
pmemset_source_set_sds(src, &sds, &state);

pmemset_map(src, map_cfg, NULL);
```

Above example presents complete capabilities of the SDS feature mentioned in
section **SHUTDOWN STATE**.

```c
struct pmemset_config *cfg;
pmemset_config_new(&cfg);
uint64_t acceptable_states = (PMEMSET_PART_STATE_OK |
				PMEMSET_PART_STATE_OK_BUT_ALREADY_OPEN);
pmemset_config_set_acceptable_states(cfg, acceptable_states);
```

Above code fragment defines *acceptable_states* as a bitmask of *PMEMSET_PART_STATE_OK*
and *PMEMSET_PART_STATE_OK_BUT_ALREADY_OPEN* states and sets it in the created
config *cfg* as a list of acceptable states.

```c
const char *file = "somefile";
struct pmemset_sds sds = PMEMSET_SDS_INITIALIZE();
struct pmemset_source *src;
enum pmemset_part_state state;

pmemset_source_from_file(&src, file);
pmemset_source_set_sds(src, &sds, &state);
```

The lines above initialize SDS *sds* structure for the first time with a
*PMEMSET_SDS_INITIALIZE* macro. **pmemset_source_set_sds**(3) function sets *sds* in
the source *src* structure along with an address of *state*. Subsequent mappings
created from *src* will have its state evaluated and returned to the *state* variable.

# SEE ALSO #

**pmemset_config_set_acceptable_states**(3),
**pmemset_config_set_event_callback**(3),
**pmemset_map**(3), **pmemset_source_from_file**(3)
**pmemset_source_set_sds**(3),
 **libpmem2**(7), **libpmem2_unsafe_shutdown**(7),
**libpmemset**(7) and **<https://pmem.io>**

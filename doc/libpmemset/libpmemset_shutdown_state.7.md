---
draft: false
slider_enable: true
description: ""
disclaimer: "The contents of this web site and the associated <a href=\"https://github.com/pmem\">GitHub repositories</a> are BSD-licensed open source."
aliases: ["libpmemset_shutdown_state.7.html"]
title: "libpmemset | PMDK"
header: "pmemset API version 1.0"
---

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2021-2022, Intel Corporation)

[comment]: <> (libpmemset_shutdown_state.7 -- man page for libpmemset shutdown state)

[NAME](#name)<br />
[DESCRIPTION](#description)<br />
[SHUTDOWN STATE](#shutdown-state)<br />
[EXAMPLE](#example)<br />
[SEE ALSO](#see-also)

# NAME #

**libpmemset_shutdown_state** - libpmemset shutdown state

# DESCRIPTION #

Persistent memory support provides a mechanism that protects the data from being
lost in case of *a power-failure*. This mechanism can fail, in that case, failure
will cause the system to shutdown in unsafe manner with the possibility of data loss.
This behavior can be discovered by checking the *unsafe shutdown counter*.
For more information about this mechanism, see **libpmem2_unsafe_shutdown**(7).

>Note: Unsafe shutdown count is supported only on the hardware with
SMART (Self-Monitoring, Analysis and Reporting Technology) monitoring system included.

# SHUTDOWN STATE #

*Shutdown state* (**SDS**) feature relies on the retrieval of *unsafe shutdown count*
from the underlying hardware. **SDS** is evaluated by comparing **SDS** values
set in the source and **SDS** information retrieved during the mapping process.
This is performed for each new mapping created using **pmemset_map**(3) from a
source *struct pmemset_source* that has a *struct pmemset_sds* structure set using
**pmemset_source_set_sds**(3) function.

When setting the **SDS** in the source, the user can optionally provide a part
*shutdown state* enum pointer *enum pmemset_part_state* with the **SDS** structure.
The library will then use this pointer to return *shutdown state* of a part.
For a complete list of possible states, see **pmemset_config_set_acceptable_states**(3).

When a part *shutdown state* is evaluated as anything but **PMEMSET_PART_STATE_OK** or
**PMEMSET_PART_STATE_OK_BUT_ALREADY_OPEN**, by default it will cause an error and
terminate the mapping process. The user can provide a list of acceptable part states
using **pmemset_config_set_acceptable_states**(3) function. Part *shutdown states*
that are set to this list will not cause an error.

**SDS** values can change during the usage of **libpmemset**(7) library, each time
an **SDS** is updated a *PMEMSET_EVENT_SDS_UPDATE* event is triggered. Updated **SDS**
can be obtained via callback function for event *PMEMSET_EVENT_SDS_UPDATE*
For more information about events, see **pmemset_config_set_event_callback**(3).

# EXAMPLE #

Example usage of the SDS feature:

```c
static int
save_sds(struct pmemset *set, struct pmemset_event_context *ctx,
		void *arg)
{
	/* make sure its the PMEMSET_EVENT_SDS_UPDATE event */
	if (ctx->type == PMEMSET_EVENT_SDS_UPDATE) {
		/* retrieve sds and correlated source */
		struct pmemset_sds *updated_sds = ctx->data.sds_update.sds;
		struct pmemset_source *related_source = ctx->data.sds_update;

		/* save the sds */
		...
	}

	return 0;
}

...
{
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
				PMEMSET_PART_STATE_OK_BUT_INTERRUPTED);
	pmemset_config_set_acceptable_states(cfg, acceptable_states);
	pmemset_config_set_event_callback(cfg, &save_sds, NULL);

	pmemset_new(&set, cfg);

	pmemset_map_config_new(&map_cfg, set);

	pmemset_source_from_file(&src, file);
	pmemset_source_set_sds(src, &sds, &state);

	pmemset_map(src, map_cfg, NULL);
}
```

Above example presents complete capabilities of the SDS feature mentioned in
section **SHUTDOWN STATE**.

```c
uint64_t acceptable_states = (PMEMSET_PART_STATE_OK |
				PMEMSET_PART_STATE_OK_BUT_INTERRUPTED);
pmemset_config_set_acceptable_states(cfg, acceptable_states);
```

Above code fragment defines *acceptable_states* as a bitmask of *PMEMSET_PART_STATE_OK*
and *PMEMSET_PART_STATE_OK_BUT_INTERRUPTED* states and sets it in the created
config *cfg* as a list of acceptable states.

```c
static int
save_sds(struct pmemset *set, struct pmemset_event_context *ctx,
		void *arg)
{
	/* make sure its the PMEMSET_EVENT_SDS_UPDATE event */
	if (ctx->type == PMEMSET_EVENT_SDS_UPDATE) {
		/* retrieve sds and correlated source */
		struct pmemset_sds *updated_sds = ctx->data.sds_update.sds;
		struct pmemset_source *related_source = ctx->data.sds_update;

		/* save the sds */
		...
	}

	return 0;
}

	...
	pmemset_config_set_event_callback(cfg, &save_sds, NULL);
```

This fragment defines a *save_sds* function that checks if the event type matches
*PMEMSET_EVENT_SDS_UPDATE* and saves the updated SDS wherever user wants. *save_sds*
is then set in a config as a callback for occurring events.

```c
struct pmemset_sds sds = PMEMSET_SDS_INITIALIZE();

pmemset_source_from_file(&src, file);
pmemset_source_set_sds(src, &sds, &state);
```

The lines above initialize SDS *sds* structure for the first time with a
*PMEMSET_SDS_INITIALIZE* macro. Instead of the initialization, the user can set *sds*
to the values retrieved with callback function *save_sds*. **pmemset_source_set_sds**(3)
function sets *sds* in the source *src* structure along with an address of *state*.
Subsequent mappings created from *src* will have its state evaluated and returned to
the *state* variable.

# SEE ALSO #

**pmemset_config_set_acceptable_states**(3),
**pmemset_config_set_event_callback**(3),
**pmemset_map**(3), **pmemset_source_from_file**(3)
**pmemset_source_set_sds**(3),
 **libpmem2**(7), **libpmem2_unsafe_shutdown**(7),
**libpmemset**(7) and **<https://pmem.io>**

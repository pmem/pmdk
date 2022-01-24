---
layout: manual
Content-Style: 'text/css'
title: _MP(PMEMSET_SOURCE_SET_SDS, 3)
collection: libpmemset
header: PMDK
date: pmemset API version 1.0
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2021-2022, Intel Corporation)

[comment]: <> (pmemset_source_set_sds.3 -- man page for pmemset_source_set_sds)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[ERRORS](#errors)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmemset_source_set_sds**() - store SDS parameter in the source structure

# SYNOPSIS #

```c
#include <libpmemset.h>

#define PMEMSET_SDS_DEVICE_ID_LEN ((size_t)512ULL)

PMEMSET_SDS_INITIALIZE()

struct pmemset_sds {
	char id[PMEMSET_SDS_DEVICE_ID_LEN + 1];
	uint64_t usc;
	int refcount;
};

enum pmemset_part_state {
	PMEMSET_PART_STATE_INDETERMINATE = (1 << 0),
	PMEMSET_PART_STATE_OK = (1 << 1),
	PMEMSET_PART_STATE_OK_BUT_INTERRUPTED = (1 << 2),
	PMEMSET_PART_STATE_CORRUPTED = (1 << 3),
};

struct pmemset_source;
int pmemset_source_set_sds(struct pmemset_source *src, struct pmemset_sds *sds,
		enum pmemset_part_state *state_ptr);
```

# DESCRIPTION #

The **pmemset_source_set_sds**() stores SDS parameter *sds* in the source *src*
structure.

Shutdown data state parameter *sds* can be initialized for the first time using
**PMEMSET_SDS_INITIALIZE**() macro. Subsequent shutdown states *sds* can be retrieved via
*PMEMSET_EVENT_SDS_UPDATE* event fired every time an *sds* is updated. For more information
please see **pmemset_config_set_event_callback**(3).

When creating a new mapping using **pmemset_map**(3) function, current part state will
be returned via provided *state_ptr* variable.

Example usage of the **pmemset_source_set_sds**():

```c
struct pmemset_sds sds = PMEMSET_SDS_INITIALIZE();

enum pmemset_part_state state;

const char *filepath = "somefile";
pmemset_source_from_file(&src, filepath);
pmemset_source_set_sds(src, &sds, &state);
```

Above code initializes the *struct pmemset_sds* structure and sets it in the source.
Any mapping created from this source will be supported by the SDS feature, *state_ptr*
variable will be updated with every new mapping from source *src*.

SDS feature is supported only on the hardware with SMART (Self-Monitoring, Analysis and Reporting Technology)
monitoring system included.

# RETURN VALUE

The **pmemset_source_set_sds**() function returns 0 on success or a negative error
code on failure.

# ERRORS #

The **pmemset_source_set_sds**() can fail with the following errors:

* **PMEMSET_E_SDS_ALREADY_SET** - SDS was already set in the source *src*

* **-ENOMEM** - out of memory

# SEE ALSO #

**pmemset_config_set_event_callback**(),
**pmemset_map**(3),
**libpmemset**(7) and **<http://pmem.io>**

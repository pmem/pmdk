---
layout: manual
Content-Style: 'text/css'
title: _MP(PMEMSET_SOURCE_SET_EXTRAS, 3)
collection: libpmemset
header: PMDK
date: pmemset API version 1.0
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2021, Intel Corporation)

[comment]: <> (pmemset_source_set_extras.3 -- man page for pmemset_source_set_extras)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[ERRORS](#errors)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmemset_source_set_extras**() - store extra parameters in the source structure

# SYNOPSIS #

```c
#include <libpmemset.h>

#define PMEMSET_SDS_DEVICE_ID_LEN ((size_t)512ULL)

struct pmemset_sds {
	char id[PMEMSET_SDS_DEVICE_ID_LEN];
	uint64_t usc;
	int refcount;
};

struct pmemset_extras {
	struct pmemset_sds *sds;
	struct pmemset_badblock *bb;
	enum pmemset_part_state *state;
};

struct pmemset_source;
void pmemset_source_set_extras(struct pmemset_source *src,
		struct pmemset_extras *ext);
```

# DESCRIPTION #

The **pmemset_source_set_extras**() stores extra parameters *ext* in the source
*src* structure.

Shutdown data state parameter *sds* can be initialized for the first time by setting
the refcount value to 0. Subsequent shutdown states *sds* can be retrieved via
*PMEMSET_EVENT_SDS_UPDATE* event fired everytime an *sds* is updated. For more information
please see **pmemset_config_set_event_callback**(3).

Bad block parameter *bb* is not yet supported.

When creating a new mapping using **pmemset_part_map**(3) function from a source with
*state* set in *ext*, a current part state will be returned via *state* variable.

**pmemset_source_set_extras**() sets *sds* and *bb* variables to NULL.

# RETURN VALUE

The **pmemset_source_set_extras**() function returns 0 on success or a negative error
code on failure.

# ERRORS #

The **pmemset_source_set_extras**() can fail with the following errors:

* **PMEMSET_E_EXTRAS_ALREADY_SET** - extras were already set in the source *src*

* **-ENOMEM** - out of memory

# SEE ALSO #

**pmemset_config_set_event_callback**(),
**pmemset_part_map**(3),
**libpmemset**(7) and **<http://pmem.io>**

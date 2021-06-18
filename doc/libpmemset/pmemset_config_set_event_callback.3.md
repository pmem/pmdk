---
layout: manual
Content-Style: 'text/css'
title: _MP(pmemset_config_set_event_callback, 3)
collection: libpmemset
header: PMDK
date: pmemset API version 1.0
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2021, Intel Corporation)

[comment]: <> (pmemset_config_set_event_callback.3 -- man page for pmemset_config_set_event_callback)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[EVENTS](#events)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmemset_config_set_event_callback**() - set an event callback

# SYNOPSIS #

```c
#include <libpmemset.h>
#define PMEMSET_EVENT_CONTEXT_SIZE (64)
struct pmemset_event_context {
	enum pmemset_event type;
	union {
		char _data[PMEMSET_EVENT_CONTEXT_SIZE];
		struct pmemset_event_copy copy;
		struct pmemset_event_flush flush;
		struct pmemset_event_drain drain;
		struct pmemset_event_persist persist;
		struct pmemset_event_bad_block bad_block;
		struct pmemset_event_part_remove part_remove;
		struct pmemset_event_part_add part_add;
	} data;
};

typedef int pmemset_event_callback(struct pmemset *set,
        struct pmemset_event_context *ctx,
        void *arg);

void pmemset_config_set_event_callback(struct pmemset_config *cfg,
		pmemset_event_callback *callback, void *arg);

```

# DESCRIPTION #

The **pmemset_config_set_event_callback**() sets an user provided *callback* in *cfg*.
*arg* will be passed to the *callback* each time it will be called by the library.

The callback will be called by *pmemset* each time an event occurs.
Events are only fired during the user's calls of the **libpmemset**(7) methods.
The detailed list of events and its description can be found in *Events* section below.
The *callback* function should return 0 in case of success.
If the event supports error handling, the *callback* can return a non-zero value in case of error,
otherwise return value is ignored.
Struct *pmemset_event_context* is a tagged union, which contains all event structures, in **libpmemset**(7).
The *type* field contains information of the type of the event fired,
where the data union contains event-specific information.

There's no guarantee that accessing pointers in *ctx* inside of the callback is thread-safe.
The library user must guarantee this by not having multiple threads accessing the same region on the set.
Once the function exits *ctx* and its data are invalid.

# RETURN VALUE #

The **pmemset_config_set_event_callback**() returns no value.

# EVENTS #

* **PMEMSET_EVENT_PART_ADD** - occurs for each new part added to the pmemset
using **pmemset_part_map**(3) function.

# SEE ALSO #

**pmemset_part_map**, **libpmem2**(7),
**libpmemset**(7) and **<http://pmem.io>**

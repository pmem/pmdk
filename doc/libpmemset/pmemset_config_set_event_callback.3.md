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
		struct pmemset_event_move move;
		struct pmemset_event_set set;
		struct pmemset_event_flush flush;
		struct pmemset_event_drain drain;
		struct pmemset_event_persist persist;
		struct pmemset_event_bad_block bad_block;
		struct pmemset_event_part_remove part_remove;
		struct pmemset_event_part_add part_add;
		struct pmemset_event_sds_update;
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

```c
struct pmemset_event_flush {
	void *addr;
	size_t len;
};
```

**PMEMSET_EVENT_FLUSH** is fired before **pmemset_flush**(3) or **pmemset_persist**(3) completes its work.
The *flush* field in *data* union contains *addr* and *len* passed to those functions.
This event doesn't support error handling, which means that the value returned by the *callback* function is ignored.

**PMEMSET_EVENT_DRAIN** is fired after **pmemset_drain**(3) or **pmemset_persist**(3) completes its work.
In case of **pmemset_persist**(3) this event is fired after **PMEMSET_EVENT_FLUSH**.
This event doesn't support error handling, which means that the value returned by the *callback* function is ignored.

```c
struct pmemset_event_copy {
	void *src;
	void *dest;
	size_t len;
	unsigned flags;
};

struct pmemset_event_move {
	void *src;
	void *dest;
	size_t len;
	unsigned flags;
};

struct pmemset_event_set {
	void *dest;
	int value;
	size_t len;
	unsigned flags;
};
```

**PMEMSET_EVENT_COPY**, **PMEMSET_EVENT_MOVE**, **PMEMSET_EVENT_SET** are fired, respectively,
before **pmemset_memcpy**(3), **pmemset_memmove**(3), **pmemset_memset**(3) completed its work.
Similarly, *copy*, *move*, or *set* fields in the *data* union contain all arguments passed to these functions.
If **PMEMSET_F_MEM_NODRAIN** flag is **not** passed to these functions, a single **PMEMSET_EVENT_DRAIN**
will be fired on the end of opperation.
During these functions "flush" and "drain" operations are performed,
but they will not trigger any additional events.
**PMEMSET_EVENT_FLUSH** and **PMEMSET_EVENT_DRAIN**
This event doesn't support error handling, which means that the value returned by the *callback* function is ignored.

```c
struct pmemset_event_part_add {
	void *addr;
	size_t len;
	struct pmem2_source *src;
};
```

**PMEMSET_EVENT_PART_ADD** is fired for each new part added to the pmemset,
after **pmemset_part_map**(3) completes its work. The *part_add* field in *data* union
contains address *addr* and length *len* of the new part and a source *src* from which
it was created.

```c
struct pmemset_event_part_remove {
	void *addr;
	size_t len;
};
```

**PMEMSET_EVENT_PART_REMOVE** is fired for each part mapping removed from the
pmemset, before **pmemset_remove_part_map**(3) function completes its work. The *part_remove*
field in *data* union contains *addr* and *len* of the part to be removed.

```c
struct pmemset_event_remove_range {
	void *addr;
	size_t len;
};
```

**PMEMSET_EVENT_REMOVE_RANGE** is fired for each range removed from the pmemset,
before **pmemset_remove_range**(3) function completes its work. The *remove_range*
field in *data* union contains *addr* and *len* of the range to be removed.
This event can trigger **PMEMSET_EVENT_PART_REMOVE** for each whole part mapping
that is removed from the set as a result of the removed range.

```c
struct pmemset_event_sds_update {
	struct pmemset_sds *sds;
	struct pmemset_source *src;
};
```

**PMEMSET_EVENT_SDS_UPDATE** is fired after each change made to any shutdown data state structure
provided by the user.
Fields *sds* and *src* correspond respectively to the SDS structure and a source it corresponds to.

# SEE ALSO #

**pmemset_map**, **libpmem2**(7),
**libpmemset**(7) and **<http://pmem.io>**

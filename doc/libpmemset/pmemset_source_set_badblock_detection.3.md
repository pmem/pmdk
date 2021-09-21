---
layout: manual
Content-Style: 'text/css'
title: _MP(PMEMSET_SOURCE_SET_BADBLOCK_DETECTION, 3)
collection: libpmemset
header: PMDK
date: pmemset API version 1.0
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2021, Intel Corporation)

[comment]: <> (pmemset_source_set_badblock_detection.3 -- man page for pmemset_source_set_badblock_detection)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmemset_source_set_badblock_detection**() - sets bad block detection feature on/off

# SYNOPSIS #

```c
#include <libpmemset.h>

struct pmemset_source;
void pmemset_source_set_badblock_detection(struct pmemset_source *src,
		bool value);
```

# DESCRIPTION #

The **pmemset_source_set_badblock_detection**() sets bad block detection feature to the provided
boolean *value*. This feature is turned off by default.

When bad block detection is set, then **pmemset_map**(3) operation searches for bad blocks in
the *src* and fires **PMEMSET_EVENT_BADBLOCK** event for each bad block found.

Encountered bad blocks can be retrieved via event callback function, see
**pmemset_config_set_event_callback**(3) for details. Bad blocks can be cleared using
**pmemset_badblock_clear**(3) function.

If all bad blocks are immediately cleared in the event callback function, then a
**PMEMSET_EVENT_BADBLOCKS_CLEARED** event is fired for a given source *src*.

# RETURN VALUE

The **pmemset_source_set_badblock_detection**() function returns no value.

# SEE ALSO #

**pmemset_badblock_clear**(3),
**pmemset_map**(3), **libpmemset**(7) and **<http://pmem.io>**

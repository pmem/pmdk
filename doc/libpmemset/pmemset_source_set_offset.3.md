---
layout: manual
Content-Style: 'text/css'
title: _MP(PMEMSET_SOURCE_SET_OFFSET, 3)
collection: libpmemset
header: PMDK
date: pmemset API version 1.0
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2021, Intel Corporation)

[comment]: <> (pmemset_source_set_offset.3 -- man page for pmemset_source_set_offset)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[ERRORS](#errors)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmemset_source_set_offset**() - set offset of mapping in the source structure.

# SYNOPSIS #

```c
#include <libpmemset.h>

int pmemset_source_set_offset(struct pmemset_source *src,
	size_t offset);
```

# DESCRIPTION #

The **pmemset_source_set_offset**() sets offset *offset* for future mapping in the *src* structure.

New mapping is created using **pmemset_map**(3) function and the offset specified in the *offset* value.

# RETURN VALUE

The **pmemset_source_set_offset**() function returns 0 on success
or a negative error code on failure.

# ERRORS #

The **pmemset_source_set_offset**() can fail with the following errors:

* **PMEMSET_E_OFFSET_OUT_OF_RANGE** - argument out of range, offset is greater than
**INT64_MAX**

# SEE ALSO #

**pmemset_map**(3), **libpmemset**(7) and **<http://pmem.io>**

---
layout: manual
Content-Style: 'text/css'
title: _MP(PMEMSET_SOURCE_SET_LENGTH, 3)
collection: libpmemset
header: PMDK
date: pmemset API version 1.0
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2021, Intel Corporation)

[comment]: <> (pmemset_source_set_length.3 -- man page for pmemset_source_set_length)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmemset_source_set_length**() - set length of mapping in the source structure.

# SYNOPSIS #

```c
#include <libpmemset.h>

void pmemset_source_set_length(struct pmemset_source *src,
	size_t length);
```

# DESCRIPTION #

The **pmemset_source_set_length**() function configures the length which will be used
for mapping. The *\*src* should be already initialized, please see **pmemset_source_from_file**(3),
**pmemset_source_from_pmem2**(3) or **pmemset_source_from_temporary**(3) for details.
The *\length* must be a multiple of the alignment required for the data
source which will be used for mapping alongside the config.
By default, the length is equal to the size of the file that is being mapped.

# RETURN VALUE

The **pmemset_source_set_length**() function returns no value.

# SEE ALSO #

**pmemset_map**(3), **pmemset_source_from_file**(3),
**pmemset_source_from_pmem2**(3) or **pmemset_source_from_temporary**(3),
**libpmemset**(7) and **<http://pmem.io>**

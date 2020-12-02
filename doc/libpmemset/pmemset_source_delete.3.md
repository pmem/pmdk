---
layout: manual
Content-Style: 'text/css'
title: _MP(PMEMSET_SOURCE_DELETE, 3)
collection: libpmemset
header: PMDK
date: pmemset API version 1.0
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2020, Intel Corporation)

[comment]: <> (pmemset_source_delete.3 -- man page for pmemset_source_delete)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmemset_source_delete**() - delete an instance of persistent memory data source

# SYNOPSIS #

```c
#include <libpmemset.h>

struct pmemset_source;

int pmemset_source_delete(struct pmemset_source **src);

```

# DESCRIPTION #

The **pmemset_source_delete**() function frees the data source obtained using either
**pmemset_source_from_file**(3) or **pmemset_source_from_pmem2**(3) function.
The user-provided variable pointed by *\*src* is set to NULL.
If *\*src* is NULL, no operation is performed.

# RETURN VALUE #

The **pmemset_source_delete**() function always returns 0.

# SEE ALSO #

**pmemset_source_from_file**(3), **pmemset_source_from_pmem2**(3),
**libpmemset**(7) and **<http://pmem.io>**

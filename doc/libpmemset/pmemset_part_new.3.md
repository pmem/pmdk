---
layout: manual
Content-Style: 'text/css'
title: _MP(PMEMSET_PART_NEW, 3)
collection: libpmemset
header: PMDK
date: pmemset API version 1.0
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2020, Intel Corporation)

[comment]: <> (pmemset_part_new.3 -- man page for libpmemset pmemset_part_new operation)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[ERRORS](#errors)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmemset_part_new**() - not supported.
# SYNOPSIS #

```c
#include <libpmem2.h>

struct pmemset;
struct pmemset_part;
struct pmemset_source;
pmemset_part_new(struct pmemset_part **part, struct pmemset *set,
		struct pmemset_source *src, size_t offset, size_t length);
```

# DESCRIPTION #

The **pmemset_part_new**() is not supported.

For the operation to succeed the *src* structure must have the path to a valid,
existing file set. See **pmemset_source_from_file**(3) for details.

# RETURN VALUE #

The **pmemset_source_new**() function returns PMEMSET_E_NOSUPP when no errors occured
or a negative error code on failure.

# ERRORS #

The **pmemset_part_new**() can fail with the following errors:

* **PMEMSET_E_INVALID_PATH** - the path to the file set in the provided *src* structure
points to invalid file.

# SEE ALSO #

**pmemset_source_from_file**(3), **libpmemset**(7) and **<http://pmem.io>**

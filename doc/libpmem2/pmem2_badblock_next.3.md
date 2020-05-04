---
layout: manual
Content-Style: 'text/css'
title: _MP(pmem2_badblock_next, 3)
collection: libpmem2
header: PMDK
date: pmem2 API version 1.0
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2020, Intel Corporation)

[comment]: <> (pmem2_badblock_next.3 -- man page for pmem2_badblock_next

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmem2_badblock_next**() - read the next bad block for the given bad block
context *\*bbctx*.

# SYNOPSIS #

```c
#include <libpmem2.h>

struct pmem2_badblock;
struct pmem2_badblock_context;

int pmem2_badblock_next(
		struct pmem2_badblock_context *bbctx,
		struct pmem2_badblock *bb)
```

# DESCRIPTION #

The **pmem2_badblock_next**() function reads the next bad block for the given
bad block context *\*bbctx*.

# RETURN VALUE #

The **pmem2_badblock_next**() function returns 0 and stores the next bad block
in *\*bb* on success or it returns a negative error code when there are no more
bad blocks for the given bad block context *\*bbctx*.

Please see **libpmem2**(7) for detailed description of libpmem2 error codes.

# ERRORS #
**pmem2_badblock_next**() can fail with the following error:

* **PMEM2_E_NO_BAD_BLOCK_FOUND** - there are no more bad blocks for the given
bad block context *\*bbctx*.

# SEE ALSO #

**pmem2_badblock_context_new**(3), **pmem2_badblock_clear**(3),
**libpmem2**(7) and **<http://pmem.io>**

---
draft: false
slider_enable: true
description: ""
disclaimer: "The contents of this web site and the associated <a href=\"https://github.com/pmem\">GitHub repositories</a> are BSD-licensed open source."
aliases: ["pmem2_badblock_next.3.html"]
title: "libpmem2 | PMDK"
header: "pmem2 API version 1.0"
---

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2020-2023, Intel Corporation)

[comment]: <> (pmem2_badblock_next.3 -- man page for pmem2_badblock_next)

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
		struct pmem2_badblock *bb);
```

# DESCRIPTION #

The **pmem2_badblock_next**() function reads the next bad block for the given
bad block context *\*bbctx*.

# RETURN VALUE #

The **pmem2_badblock_next**() function returns 0 and stores the next bad block
in *\*bb* on success or it returns a negative error code when there are no more
bad blocks for the given bad block context *\*bbctx*.

# ERRORS #

**pmem2_badblock_next**() can fail with the following error:

* **PMEM2_E_NO_BAD_BLOCK_FOUND** - there are no more bad blocks for the given
bad block context *\*bbctx*, *\*bb* is undefined in this case.

* **PMEM2_E_NOSUPP** - when the OS does not support this functionality

# SEE ALSO #

**pmem2_badblock_context_new**(3), **pmem2_badblock_clear**(3),
**libpmem2**(7) and **<https://pmem.io>**

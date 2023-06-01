---
draft: false
slider_enable: true
description: ""
disclaimer: "The contents of this web site and the associated <a href=\"https://github.com/pmem\">GitHub repositories</a> are BSD-licensed open source."
aliases: ["pmem2_badblock_clear.3.html"]
title: "libpmem2 | PMDK"
header: "pmem2 API version 1.0"
---

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2020-2023, Intel Corporation)

[comment]: <> (pmem2_badblock_clear.3 -- man page for pmem2_badblock_clear)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[ERRORS](#errors)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmem2_badblock_clear**() - clear the given bad block

# SYNOPSIS #

```c
#include <libpmem2.h>

struct pmem2_badblock;
struct pmem2_badblock_context;

int pmem2_badblock_clear(
		struct pmem2_badblock_context *bbctx,
		struct pmem2_badblock *bb);
```

# DESCRIPTION #

The **pmem2_badblock_clear**() function clears the given *\*bb* bad block.

It means that the **pmem2_badblock_clear**() function unmaps bad blocks
and maps new, healthy, blocks in place of the bad ones.
The new blocks are zeroed. The content of the bad blocks is lost.

# RETURN VALUE #

The **pmem2_badblock_clear**() function clears the given *\*bb* bad block
and returns 0 on success or a negative error code on failure.

# ERRORS #

**pmem2_badblock_clear**() can fail with the following errors:

* **PMEM2_E_OFFSET_OUT_OF_RANGE** - bad block's offset is greater than INT64_MAX

* **PMEM2_E_LENGTH_OUT_OF_RANGE** - bad block's length is greater than INT64_MAX

* **-errno** - set by failing **fallocate**(2), while deallocating bad blocks
or allocating new blocks

* **-errno** - set by failing ndctl functions: **ndctl_bus_cmd_new_ars_cap**,
**ndctl_cmd_submit**, **ndctl_cmd_ars_cap_get_range** or
**ndctl_bus_cmd_new_clear_error** while trying to clear a bad block
in a DAX device

* **-ENXIO** - **ndctl_bus_cmd_new_clear_error** did not manage to clear
all bad blocks

# SEE ALSO #

**pmem2_badblock_context_new**(3), **pmem2_badblock_next**(3),
**libpmem2**(7) and **<https://pmem.io>**

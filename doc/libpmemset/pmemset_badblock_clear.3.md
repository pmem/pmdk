---
draft: false
slider_enable: true
description: ""
disclaimer: "The contents of this web site and the associated <a href=\"https://github.com/pmem\">GitHub repositories</a> are BSD-licensed open source."
aliases: ["pmemset_badblock_clear.3.html"]
title: "libpmemset | PMDK"
header: "pmemset API version 1.0"
---

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2021, Intel Corporation)

[comment]: <> (pmemset_badblock_clear.3 -- man page for libpmemset pmemset_badblock_clear operation)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[ERRORS](#errors)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmemset_badblock_clear**() - clears a bad block

# SYNOPSIS #

```c
#include <libpmemset.h>

struct pmemset_badblock {
    size_t offset;
    size_t length;
};

struct pmemset_source;
int pmemset_badblock_clear(struct pmemset_badblock *bb,
		struct pmemset_source *src);
```

# DESCRIPTION #

The **pmemset_badblock_clear**() function clears provided bad block *\*bb*.

This function relies on the **libmem2**(7) functions **pmem2_badblock_clear**(3)
and **pmem2_badblock_context_new**(3) for bad block clearing.

The content of the bad blocks is lost upon clearing, for detailed information about
bad block clearing, see **pmem2_badblock_clear**(3).

It is not supported on Windows.

# RETURN VALUE #

The **pmemset_badblock_clear**() function returns 0 on success or a negative
error code on failure.

# ERRORS #

* **PMEMSET_E_DAX_REGION_NOT_FOUND** - cannot find a DAX region for the given *src*.

* **PMEMSET_E_CANNOT_READ_BOUNDS** - cannot read offset or size of the namespace
of the given *src*.

* **PMEMSET_E_BADBLOCK_NOSUPP** - bad blocks handling isn't supported on this OS.

* **PMEMSET_E_OFFSET_OUT_OF_RANGE** - bad block offset is greater than INT64_MAX

* **PMEMSET_E_LENGTH_OUT_OF_RANGE** - bad block length is greater than INT64_MAX

* **-errno** - set by failing underlying **pmem2_badblock_context_new**(3) function.

* **-errno** - set by failing underlying **pmem2_badblock_clear**(3) function.

# SEE ALSO #

**pmem2_badblock_clear**(3), **pmem2_badblock_context_new**(3),
**libpmem2**(7), **libpmemset**(7) and **<http://pmem.io>**

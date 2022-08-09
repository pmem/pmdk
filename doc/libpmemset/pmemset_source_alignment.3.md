---
draft: false
slider_enable: true
description: ""
disclaimer: "The contents of this web site and the associated <a href=\"https://github.com/pmem\">GitHub repositories</a> are BSD-licensed open source."
aliases: ["pmemset_source_alignment.3.html"]
title: "libpmemset | PMDK"
header: "pmemset API version 1.0"
---

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2021-2022, Intel Corporation)

[comment]: <> (pmemset_source_alignment.3 -- man page for pmemset_source_alignment)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[ERRORS](#errors)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmemset_source_alignment**() - returns data source alignment

# SYNOPSIS #

```c
#include <libpmemset.h>

struct pmemset_source;

int pmemset_source_alignment(struct pmemset_source *src, size_t *alignment);

```

# DESCRIPTION #

The **pmemset_source_alignment**() function retrieves the alignment of offset and
length needed for **pmemset_map**(3) to succeed. The alignment is stored in
*\*alignment* argument.

# RETURN VALUE #

The **pmemset_source_alignment**() function returns 0 on success.
If the function fails, the *\*alignment* variable is left unmodified and a negative
error code is returned.

# ERRORS #

The **pmemset_source_alignment**() can fail with the following errors:

* **PMEMSET_E_INVALID_ALIGNMENT_VALUE** - cannot read alignment for source file
from the *src*.

# SEE ALSO #

**pmemset_map**(3), **libpmemset**(7) and **<http://pmem.io>**

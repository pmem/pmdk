---
draft: false
slider_enable: true
description: ""
disclaimer: "The contents of this web site and the associated <a href=\"https://github.com/pmem\">GitHub repositories</a> are BSD-licensed open source."
aliases: ["pmem2_source_from_anon.3.html"]
title: "libpmem2 | PMDK"
header: "pmem2 API version 1.0"
---

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2020, Intel Corporation)

[comment]: <> (pmem2_source_from_anon.3 -- man page for pmem2_source_from_anon

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[ERRORS](#errors)<br />
[CAVEATS](#caveats)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmem2_source_from_anon**() - creates data source backed by anonymous memory pages

# SYNOPSIS #

```c
#include <libpmem2.h>

int pmem2_source_from_anon(struct pmem2_source **src, size_t size);
```

# DESCRIPTION #

The **pmem2_source_from_anon**() function instantiates a new *struct pmem2_source*
object describing an anonymous data source. Mappings created using this function
are not backed by any file and are zero-initialized.

The *size* argument for the function defines the length in bytes of the anonymous
source, as returned by **pmem2_source_size**(3). The application should set
this value so that it's greater than or equal to the size of any mapping created
with the anonymous source.

The offset value for mapping is ignored.

# RETURN VALUE #

The **pmem2_source_from_anon**() function returns 0 on success
or a negative error code on failure.

# ERRORS #

The **pmem2_source_form_anon**() can fail with the following errors:

* **-ENOMEM** - in case of insufficient
memory to allocate an instance of *struct pmem2_source*.

# SEE ALSO #
**errno**(3), **pmem2_config_set_length**(3), **pmem2_map_new**(3),
**pmem2_source_size**(3), **pmem2_config_set_length**(3), **libpmem2**(7)
and **<https://pmem.io>**

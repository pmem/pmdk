---
layout: manual
Content-Style: 'text/css'
title: _MP(PMEM2_SOURCE_FROM_FD, 3)
collection: libpmem2
header: PMDK
date: pmem2 API version 1.0
...

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

int pmem2_source_from_anon(struct pmem2_source *src);
```

# DESCRIPTION #

The **pmem2_source_from_anon**() function instantiates a new *struct pmem2_source**
object describing the anonymous data source. Mappings created using this function
are not backed by any file and are zero-initialized.

The size of the anonymous source, as defined by **pmem2_source_size**(3), is equal
to the maximum theoretical size of the virtual address space. Because of this,
the **pmem2_map**(3) function is likely to fail if the length of the mapping is not
explicitly set in the *pmem2_config* structure.

The offset value for mapping, if set, is ignored.

# RETURN VALUE #

**pmem2_source_from_anon**() functions return 0 on success or one of the error
values listed in the next section.

# ERRORS #

The **pmem2_source_from_anon**() can return **-ENOMEM** in case of insufficient
memory to allocate an instance of *struct pmem2_source*.

# SEE ALSO #
**errno**(3), **pmem2_map**(3), **pmem2_source_size**(3),
**pmem2_config_set_length**(3), **libpmem2**(7)
and **<http://pmem.io>**

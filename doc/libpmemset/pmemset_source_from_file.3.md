---
layout: manual
Content-Style: 'text/css'
title: _MP(PMEMSET_SOURCE_FROM_FILE, 3)
collection: libpmemset
header: PMDK
date: pmemset API version 1.0
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2020, Intel Corporation)

[comment]: <> (pmemset_source_from_file.3 -- man page for pmemset_source_from_file)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[ERRORS](#errors)<br />
[SEE ALSO](#see-also)<br />

# NAME #

_UW(pmemset_source_from_file)
- creates an instance of persistent memory data source

# SYNOPSIS #

```c
#include <libpmemset.h>

_UWFUNCR1(int, pmemset_source_from_file, struct pmemset_source **src, const char *file)
```

# DESCRIPTION #

_UW(pmemset_source_from_file) function instantiates a new *struct pmemset_source** object
describing the data source and sets a path to the file in it.

Obtained source is ready to be passed on to the **pmemset_part_new**() function.
See **pmemset_part_new**(3) for details.

# RETURN VALUE #

The _UW(pmemset_source_from_file) functions return 0 on success or
a negative error code on failure.

# ERRORS #

The _UW(pmemset_source_from_file) can fail with the following errors:

* **PMEMSET_E_INVALID_FILE_PATH** - when the provided file path string is NULL.

* **-ENOMEM** - in case of insufficient memory to allocate an instance
of *struct pmemset_source*.

# SEE ALSO #

**pmemset_part_new**(3), **libpmemset**(7) and **<http://pmem.io>**

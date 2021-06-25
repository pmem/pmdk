---
layout: manual
Content-Style: 'text/css'
title: PMEMSET_SOURCE_FROM_FILE
collection: libpmemset
header: PMDK
date: pmemset API version 1.0
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2020-2021, Intel Corporation)

[comment]: <> (pmemset_source_from_file.3 -- man page for pmemset_source_from_file)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[ERRORS](#errors)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmemset_source_from_fileU**()/**pmemset_source_from_fileW**()
- creates an instance of persistent memory data source

# SYNOPSIS #

```c
#include <libpmemset.h>

int pmemset_source_from_fileU(struct pmemset_source **src, const char *file);
int pmemset_source_from_fileW(struct pmemset_source **src, const wchar_t *file);
int pmemset_xsource_from_fileU(struct pmemset_source **src, const char *file, unsigned flags);
int pmemset_xsource_from_fileW(struct pmemset_source **src, const wchar_t *file, unsigned flags);
int pmemset_source_delete(struct pmemset_source **src);

```

# DESCRIPTION #

**pmemset_source_from_fileU**()/**pmemset_source_from_fileW**() function instantiates a new *struct pmemset_source** object
describing the data source and sets a path to the file in it.
**pmemset_xsource_from_fileU**()/**pmemset_xsource_from_fileW**() is equivalent to **pmemset_source_from_fileU**()/**pmemset_source_from_fileW**(), but with
additional *flags* argument that is a bitmask of the following values:

* **PMEMSET_SOURCE_FILE_CREATE_IF_NEEDED** - a new file will be created only if the specified file does not already exist,

* **PMEMSET_SOURCE_FILE_CREATE_ALWAYS** - always a new file will be created. If the specified file exists, the file will be overwritten,

* **PMEMSET_SOURCE_FILE_TRUNCATE_IF_NEEDED** - the specified file will be truncated during **pmemset_part_map**(3) to
designated part size and offset.

Obtained source is ready to be passed on to the **pmemset_part_new**() function.
See **pmemset_part_new**(3) for details.

The **pmemset_source_delete**() function frees *\*src* and sets *\*src* to NULL. If *\*src* is NULL, no operation is performed.

# RETURN VALUE #

The **pmemset_source_from_fileU**()/**pmemset_source_from_fileW**() and **pmemset_xsource_from_fileU**()/**pmemset_xsource_from_fileW**() functions
return 0 on success or  negative error code on failure.

The **pmemset_source_delete**() function always returns 0.

# ERRORS #

The **pmemset_source_from_fileU**()/**pmemset_source_from_fileW**() and **pmemset_xsource_from_fileU**()/**pmemset_xsource_from_fileW**() can fail
with the following errors:

* **PMEMSET_E_INVALID_SOURCE_PATH** - when the provided file path string is NULL.

* **-ENOMEM** - in case of insufficient memory to allocate an instance
of *struct pmemset_source*.

The **pmemset_xsource_from_fileU**()/**pmemset_xsource_from_fileW**() can also fail with the error:

* **PMEMSET_E_INVALID_SOURCE_FILE_CREATE_FLAGS** - in case of invalid *flags*
parameter.

# SEE ALSO #

**pmemset_part_map**(3), **pmemset_part_new**(3),
**libpmemset**(7) and **<http://pmem.io>**

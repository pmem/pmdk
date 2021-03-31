---
layout: manual
Content-Style: 'text/css'
title: PMEMSET_SOURCE_FROM_TEMPORARY
collection: libpmemset
header: PMDK
date: pmemset API version 1.0
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2021, Intel Corporation)

[comment]: <> (pmemset_source_from_temporary.3 -- man page for pmemset_source_from_temporary)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[ERRORS](#errors)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmemset_source_from_temporaryU**()/**pmemset_source_from_temporaryW**()
- creates an instance of persistent memory data source

# SYNOPSIS #

```c
#include <libpmemset.h>

int pmemset_source_from_temporaryU(const char struct pmemset_source **src, const char *dir);
int pmemset_source_from_temporaryW(const wchar_t struct pmemset_source **src, const char *dir);
```
# DESCRIPTION #

**pmemset_source_from_temporaryU**()/**pmemset_source_from_temporaryW**() function instantiates a new *struct pmemset_source** object
describing the data source and creates a unnamed temporary file in the provided directory *dir*.
The temporary file is always created with mode 0600, and the *dir* must specify an existing
directory name. The created file has size 0 and is extended dynamically based on required
part size during **pmemset_part_map**(3).

In case of source from temporary file the **pmemset_source_delete**(3) function
frees *\*src* and sets *\*src* to NULL and closes the temporary file as a result
the file is immediately deleted.

# RETURN VALUE #

The **pmemset_source_from_temporaryU**()/**pmemset_source_from_temporaryW**() function return 0 on success
or negative error code on failure.

# ERRORS #

The **pmemset_source_from_temporaryU**()/**pmemset_source_from_temporaryW**() can fail with the following errors:

* **PMEMSET_E_INVALID_SOURCE_PATH** - the provided directory path string is NULL
or provided path does not exists.

* **PMEMSET_E_CANNOT_CREATE_TEMP_FILE** - cannot create a unique temporary filename.

* **-ENOMEM** - in case of insufficient memory to allocate an instance
of *struct pmemset_source*.

# SEE ALSO #

**pmemset_part_map**(3), **pmemset_source_delete**(3),
**libpmemset**(7) and **<http://pmem.io>**

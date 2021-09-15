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

**pmemset_source_from_file**()
- creates an instance of persistent memory data source

# SYNOPSIS #

```c
#include <libpmemset.h>

int pmemset_source_from_file(struct pmemset_source **src, const char *file);
int pmemset_xsource_from_file(struct pmemset_source **src, const char *file, uint64_t flags);
int pmemset_source_delete(struct pmemset_source **src);

```

# DESCRIPTION #

**pmemset_source_from_file**() function instantiates a new *struct pmemset_source** object
describing the data source and sets a path to the file in it.
**pmemset_xsource_from_file**() is equivalent to **pmemset_source_from_file**(), but with
additional *flags* argument that is a bitmask of the following values:

* **PMEMSET_SOURCE_FILE_CREATE_IF_NEEDED** - a new file will be created only if the specified file does not already exist,

* **PMEMSET_SOURCE_FILE_CREATE_ALWAYS** - always a new file will be created. If the specified file exists, the file will be overwritten,

* **PMEMSET_SOURCE_FILE_DO_NOT_GROW** - the specified file will not be extended during **pmemset_map**(3). The file is extended to
designated part size and offset by default.

* **PMEMSET_SOURCE_FILE_CREATE_MODE(mode)** - the *mode* argument specifies the file mode to be
        applied when a new file is created. This macro should be applied using bitwise OR operator with the above create *flags*.
        The following *mode* values are available:
        * **PMEMSET_SOURCE_FILE_RWXU_MODE** - user has read, write, and execute permission
        * **PMEMSET_SOURCE_FILE_RWU_MODE** - user has read, write permission
        * **PMEMSET_SOURCE_FILE_RUSR_MODE** - user has read permission
        * **PMEMSET_SOURCE_FILE_WUSR_MODE** - user has write permission
        * **PMEMSET_SOURCE_FILE_XUSR_MODE** - user has execute permission
        * **PMEMSET_SOURCE_FILE_RWXG_MODE** - group has read, write, and execute permission
        * **PMEMSET_SOURCE_FILE_RWG_MODE** - group has read, write permission
        * **PMEMSET_SOURCE_FILE_RGRP_MODE** - group has read permission
        * **PMEMSET_SOURCE_FILE_WGRP_MODE** - group has write permission
        * **PMEMSET_SOURCE_FILE_XGRP_MODE** - group has execute permission
        * **PMEMSET_SOURCE_FILE_RWXO_MODE** - others have read, write, and execute permission
        * **PMEMSET_SOURCE_FILE_RWXO_MODE** - others have read, write permission
        * **PMEMSET_SOURCE_FILE_ROTH_MODE** - others have read permission
        * **PMEMSET_SOURCE_FILE_WOTH_MODE** - others have write permission
        * **PMEMSET_SOURCE_FILE_XOTH_MODE** - others have execute permission

The *mode* value can also be passed as an octal number.

If *PMEMSET_SOURCE_FILE_CREATE_MODE(mode)* is not specified then the default file mode is set
to RW for user and R for group and others.

* **PMEMSET_SOURCE_FILE_CREATE_ALWAYS_MODE(mode)** - simplified version of PMEMSET_SOURCE_FILE_CREATE_MODE and
        PMEMSET_SOURCE_FILE_CREATE_ALWAYS flag.

For example:
```
    /* bitwise OR flag with mode values */
	uint64_t flags = PMEMSET_SOURCE_FILE_CREATE_ALWAYS | \
		PMEMSET_SOURCE_FILE_CREATE_MODE(PMEMSET_SOURCE_FILE_WUSR_MODE);
	pmemset_xsource_from_file(&src, file, flags);

    /* or simplified version */
    flags = PMEMSET_SOURCE_FILE_CREATE_ALWAYS_MODE(PMEMSET_SOURCE_FILE_WUSR_MODE);
    pmemset_xsource_from_file(&src, file, flags);

    /* or version with numeric octal mode value*/
    flags = PMEMSET_SOURCE_FILE_CREATE_ALWAYS_MODE(00200);
    pmemset_xsource_from_file(&src, file, flags);
```

>NOTE: The *mode* argument is not supported on Windows.
Using *mode* values with *PMEMSET_SOURCE_FILE_CREATE_MODE(mode)* has no effects.

Obtained source is ready to be passed on to the **pmemset_map_config_new**() function.
See **pmemset_map_config_new**(3) for details.

The **pmemset_source_delete**() function frees *\*src* and sets *\*src* to NULL. If *\*src* is NULL, no operation is performed.

# RETURN VALUE #

The **pmemset_source_from_file**() and **pmemset_xsource_from_file**() functions
return 0 on success or negative error code on failure.

The **pmemset_source_delete**() function always returns 0.

# ERRORS #

The **pmemset_source_from_file**() and **pmemset_xsource_from_file**() can fail
with the following errors:

* **PMEMSET_E_INVALID_SOURCE_PATH** - when the provided file path string is NULL.

* **-ENOMEM** - in case of insufficient memory to allocate an instance
of *struct pmemset_source*.

The **pmemset_xsource_from_file**() can also fail with the error:

* **PMEMSET_E_INVALID_SOURCE_FILE_CREATE_FLAGS** - in case of invalid *flags* or *mode*
parameter.

# SEE ALSO #

**pmemset_map**(3), **pmemset_map_config_new**(3),
**libpmemset**(7) and **<http://pmem.io>**

---
layout: manual
Content-Style: 'text/css'
title: _MP(PMEMSET_CONFIG_SET_FILE_CREATE_DISPOSITION, 3)
collection: libpmemset
header: PMDK
date: pmemset API version 1.0
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2020, Intel Corporation)

[comment]: <> (pmemset_config_set_file_create_disposition.3 -- man page for pmemset_config_set_file_create_disposition)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[ERRORS](#errors)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmemset_config_set_file_create_disposition**() - set disposition when opening a file.

# SYNOPSIS #

```c
#include <libpmemset.h>

int pmemset_config_set_file_create_disposition(struct pmemset_config *cfg,
		enum pmemset_config_file_create_disposition value);
```

# DESCRIPTION #

The **pmemset_config_set_file_create_disposition**() sets file create disposition value *value*, which is used when opening a file during part new.
The disposition is configured for entire set, stred in the *pmemset_config* structure.

File create disposition *value* must be one of the following values:

 * **PMEMSET_CONFIG_FILE_CREATE_ALWAYS**

 * **PMEMSET_CONFIG_FILE_CREATE_IF_NEEDED**

 * **PMEMSET_CONFIG_FILE_OPEN**

# RETURN VALUE

The **pmemset_config_set_file_create_disposition**() function returns 0 on success or a negative error code on failure.

# ERRORS #

The **pmemset_config_set_file_create_disposition**() can fail with the following error:

* **PMEMSET_E_INVALID_CFG_FILE_CREATE_DISP** - disposition *value* is not a valid value.

# SEE ALSO #

**libpmemset**(7) and **<http://pmem.io>**

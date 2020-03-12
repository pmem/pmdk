---
layout: manual
Content-Style: 'text/css'
title: _MP(PMEM2_SOURCE_DEVICE_ID, 3)
collection: libpmem2
header: PMDK
date: pmem2 API version 1.0
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2020, Intel Corporation)

[comment]: <> (pmem2_source_device_id.3 -- man page for pmem2_source_device_id)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmem2_source_device_id**() - returns the unique identifier of a device

# SYNOPSIS #

```c
#include <libpmem2.h>

struct pmem2_source;
int pmem2_source_device_id(const struct pmem2_source *source, char *id, size_t *len);
```

# DESCRIPTION #

The **pmem2_source_device_id**() function retrieves a unique identifier
of all NVDIMMs backing the data source. This function has two operating modes:

* if *\*id* is NULL the function calculates a buffer length required for
storing the identifier of the *\*source* device and puts this length in *\*len*

* if *\*id* is not NULL it should point to a buffer of a length provided in *\*len*.
When **pmem2_source_device_id**() succeed it will store a unique identifier
of all NVDIMMs backing the data source.

For details on how to use the unique identifier for detecting *the unsafe shutdown*
please refer to **libpmem2_unsafe_shutdown**(7) manual page.

# RETURN VALUE #

The **pmem2_source_device_id**() function returns 0 on success.
If the function fails, the *\*id* and *\*len* variables contents are undefined,
and one of the following errors is returned:

On all systems:

* **PMEM2_E_BUFFER_TOO_SMALL** - *\*len* indicates the *\*id* buffer is too short.

On Windows:

* -**errno** equivalent of return code set by failing
**GetFinalPathNameByHandleW**(), while trying to resolve the volume path from the
file handle

* -**errno** set by failing **malloc**(3), while trying to allocate a buffer
for storing volume path

* -**errno** equivalent of return code set by failing
**CreateFileW**(), while trying to obtain a handle to the volume

* -**errno** equivalent of return code set by failing
**DeviceIoControl **(), while trying to obtain  volume **USC** value

* **PMEM2_E_NOSUPP** - if getting the **USC** value is not supported on the system

On Linux:

* -**errno** set by failing **fstat**(2), while trying to validate the file
descriptor.

* -**errno** set by failing **ndctl_new**(), while trying to initiate a new
NDCTL library context

On FreeBSD:

* **PMEM2_E_NOSUPP** - since it is not yet supported

# SEE ALSO #

**fstat**(2), **errno**(3), **malloc**(3), **libpmem2_unsafe_shutdown**(7),
 and **<http://pmem.io>**

---
layout: manual
Content-Style: 'text/css'
title: PMEM2_SOURCE_DEVICE_ID
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
The more hardware devices back the data source, the longer the length is.

* if *\*id* is not NULL it must point to a buffer of length *\*len* provided by
the previous call to this function.
On success, **pmem2_source_device_id**() will store a unique identifier
of all hardware devices backing the data source.

For details on how to use the unique identifier for detecting *the unsafe shutdown*
please refer to **libpmem2_unsafe_shutdown**(7) manual page.

# RETURN VALUE #

The **pmem2_source_device_id**() function returns 0 on success.
If the function fails, the *\*id* and *\*len* variables contents are left unmodified,
and one of the following errors is returned:

On all systems:

* **PMEM2_E_BUFFER_TOO_SMALL** - the provided buffer of length *\*len* is too
small to store the full identifier of the backing devices.
* **PMEM2_E_NOSUPP** - the underlying platform does not expose hardware
identification.

On Windows:

* -**errno** equivalent of return code set by failing
**GetFinalPathNameByHandleW**(), while trying to resolve the volume path from the
file handle.

* -**errno** set by failing **malloc**(3), while trying to allocate a buffer
for storing volume path.

* -**errno** equivalent of return code set by failing
**CreateFileW**(), while trying to obtain a handle to the volume.

* -**errno** equivalent of return code set by failing
**DeviceIoControl **(), while trying to obtain volume **USC** value.

On Linux:

* -**errno** set by failing **fstat**(2), while trying to validate the file
descriptor.

* -**errno** set by failing **ndctl_new**(), while trying to initiate a new
NDCTL library context.

# SEE ALSO #

**fstat**(2), **errno**(3), **malloc**(3), **libpmem2_unsafe_shutdown**(7),
 and **<http://pmem.io>**

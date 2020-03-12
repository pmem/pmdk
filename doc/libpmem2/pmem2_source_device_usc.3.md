---
layout: manual
Content-Style: 'text/css'
title: _MP(PMEM2_SOURCE_DEVICE_USC, 3)
collection: libpmem2
header: PMDK
date: pmem2 API version 1.0
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2020, Intel Corporation)

[comment]: <> (pmem2_source_device_usc.3 -- man page for pmem2_source_device_usc)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmem2_source_device_usc**() - returns the *unsafe shutdown counter* value of a
device

# SYNOPSIS #

```c
#include <libpmem2.h>

struct pmem2_source;
int pmem2_source_device_usc(const struct pmem2_source *source, uint64_t *usc);
```

# DESCRIPTION #

The **pmem2_source_device_usc**() function retrieves a sum of *unsafe shutdown counter*
(**USC**) values of all NVDIMMs backing the data source and puts the sum in *\*usc*.

For details on what **USC** is and how to use it for detecting *the unsafe shutdown*
please refer to **libpmem2_unsafe_shutdown**(7) manual page.

# RETURN VALUE #

The **pmem2_source_device_usc**() function returns 0 on success.
If the function fails, the *\*usc* variable content is undefined, and one of
the following errors is returned:

On Windows:

* -**errno** equivalent of return code set by failing
**GetFinalPathNameByHandleW**(), while trying to resolve volume path from the
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

* -**errno** set by failing **ndctl_dimm_get_dirty_shutdown **(),
while trying to obtain DIMM **USC** value

On FreeBSD:

* **PMEM2_E_NOSUPP** - since it is not yet supported

# SEE ALSO #

**fstat**(2), **errno**(3), **malloc**(3), **libpmem2_unsafe_shutdown**(7),
 and **<http://pmem.io>**

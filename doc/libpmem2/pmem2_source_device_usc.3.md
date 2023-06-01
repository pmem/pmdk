---
draft: false
slider_enable: true
description: ""
disclaimer: "The contents of this web site and the associated <a href=\"https://github.com/pmem\">GitHub repositories</a> are BSD-licensed open source."
aliases: ["pmem2_source_device_usc.3.html"]
title: "libpmem2 | PMDK"
header: "pmem2 API version 1.0"
---

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2020-2023, Intel Corporation)

[comment]: <> (pmem2_source_device_usc.3 -- man page for pmem2_source_device_usc)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[ERRORS](#errors)<br />
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

The **pmem2_source_device_usc**() function retrieves the sum of the
*unsafe shutdown count*(**USC**) values of all hardware devices backing
the data source and stores it in *\*usc*.

Please refer to **libpmem2_unsafe_shutdown**(7) for detailed description on how
to properly consume this information.

# RETURN VALUE #

The **pmem2_source_device_usc**() function returns 0 on success.
If the function fails, the *\*usc* variable content is left unmodified
and a negative error code is returned.

# ERRORS #

The **pmem2_source_device_usc**() can fail with the following errors:

* **PMEM2_E_NOSUPP** - the underlying platform does not expose unsafe shutdown
count information.

* -**errno** set by failing **fstat**(2), while trying to validate the file
descriptor.

* -**errno** set by failing **ndctl_new**(), while trying to initiate a new
NDCTL library context.

* -**errno** set by failing **ndctl_dimm_get_dirty_shutdown**(),
while trying to obtain DIMM **USC** value.

# SEE ALSO #

**fstat**(2), **errno**(3), **malloc**(3), **libpmem2_unsafe_shutdown**(7),
 and **<https://pmem.io>**

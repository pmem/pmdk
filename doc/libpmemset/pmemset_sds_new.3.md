---
layout: manual
Content-Style: 'text/css'
title: _MP(PMEMSET_SDS_NEW, 3)
collection: libpmemset
header: PMDK
date: pmemset API version 1.0
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2021, Intel Corporation)

[comment]: <> (pmemset_sds_new.3 -- man page for pmemset_sds_new and
pmemset_sds_delete

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[ERRORS](#errors)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmemset_new**(), **pmemset_delete**() - allocate and free a structure for a
pmemset shutdown data state object

# SYNOPSIS #

```c
#include <libpmemset.h>

#define PMEMSET_SDS_DEVICE_ID_LEN ((size_t)512ULL)

struct pmemset_sds;
struct pmemset_source;
int pmemset_sds_new(struct pmemset_sds **sds_ptr, struct pmemset_source *src);
int pmemset_sds_delete(struct pmemset_sds **sds_ptr);
```

# DESCRIPTION #

The **pmemset_sds_new**() function allocates and initializes a new shutdown data state
structure, *pmemset*, and returns it through the pointer in *\*sds_ptr*. Shutdown data state
describes the state of the data source *src*.

The **pmemset_sds_delete**() function frees *\*sds_ptr* returned by **pmemset_sds_new**()
and sets *\*sds_ptr* to NULL.

Obtained SDS structure can be set in the source using **pmemset_source_set_extras**(3).
Every mapping created from the source *src* with SDS structure set will be supported by
the SDS feature.

Each created part can be described by one of the following states:

* **PMEMSET_PART_STATE_INDETERMINATE** - the pool state cannot be determined because of
the errors during retrieval of device information.

* **PMEMSET_PART_STATE_OK** - the pool is internally consistent and was closed cleanly.
Application can assume that no custom recovery is needed.

* **PMEMSET_PART_STATE_OK_BUT_INTERRUPTED** - the pool is internally consistent, but it
was not closed cleanly. Application must perform consistency checking and custom recovery
on user data.

* **PMEMSET_PART_STATE_CORRUPTED** - the pool can contain invalid data as a result of hardware
failure. Reading the pool is unsafe.

When mapping a new part using **pmemset_part_map**(3) a part state is checked, if determined
state does not belong to the acceptable states, then **pmemset_part_map**(3) is terminated
and an error is returned. **PMEMSET_PART_STATE_OK** is default acceptable state for each
new SDS structure.

Acceptable states can be set inside a SDS structure using **pmemset_sds_set_acceptable_states**(3).

# RETURN VALUE #

The **pmemset_sds_new**() function returns 0 on success or a negative error code on failure.
**pmemset_sds_new**() sets *\*sds_ptr* variable to NULL on failure.

The **pmemset_sds_delete**() function always returns 0.

# ERRORS #

**pmemset_sds_new**() can fail with the following errors:

* **PMEMSET_ENOSUPP** - device that stores the data described by the source *src*
does not support SDS feature

* **-ENOMEM** - out of memory

# SEE ALSO #

**pmemset_part_map**(3),
**pmemset_sds_set_acceptable_states**(3),
**pmemset_source_set_extras**(3),
**libpmemset**(7) and **<http://pmem.io>**

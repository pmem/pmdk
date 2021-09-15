---
layout: manual
Content-Style: 'text/css'
title: PMEMSET_MAP
collection: libpmemset
header: PMDK
date: pmemset API version 1.0
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2021, Intel Corporation)

[comment]: <> (pmemset_map.3 -- man page for libpmemset pmemset_map operation)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[ERRORS](#errors)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmemset_map**() - creates a part mapping

# SYNOPSIS #

```c
#include <libpmemset.h>

struct pmemset_map_config;
struct pmemset_part_descriptor;
struct pmemset_source;
int pmemset_map(struct pmemset_source *src,
		struct pmemset_map_config *map_cfg,
		struct pmemset_part_descriptor *desc);
```

# DESCRIPTION #

The **pmemset_map**() function creates new mapping in the virtual address space
of the calling process and adds structure describing this mapping to the pmemset. It requires
an address of a pointer to initialized map configuration provided in the *map_cfg* parameter.
A map configuration should be created using **pmemset_map_config_new**(3) function.
The mapping can later be retrieved using **pmemset_first_part_map**(3),
**pmemset_next_part_map**(3) and **pmemset_part_map_by_address**(3) functions.

New mapping is created based on the source specified in the *\*src* pointer.
For the operation to succeed the *src* structure cannot be a *NULL* value and must be
created from a valid data source. See **pmemset_source_from_file**(3) and
**pmemset_source_from_pmem2**(3) for possible sources.

Optionally **pmemset_map**() function can take a part descriptor object passed via *desc* parameter.
If an optional descriptor was provided then address and size of the mapping are stored in the
descriptor when this function succeeds.

Before the initialization of pmemset, a virtual memory reservation can be set in its config structure.
This limits the future part mappings of initialized pmemset to the virtual address space spanned by the provided
reservation. Provided reservation's address and size will not be changed on pmemset operations.
For more information about this configuration please see **pmemset_config_set_reservation**(3).

During the lifespan of initialized pmemset, a contiguous part coalescing feature value can
be set using **pmemset_set_contiguous_part_coalescing**() function, modifying the default behavior of
part mapping. With contiguous part coalescing feature enabled, **pmemset_map**() function tries to map each
new part at the virtual memory region that is situated right after the previous mapped part memory range.

# RETURN VALUE #

The **pmemset_map**() function returns 0 on success
or a negative error code on failure.

# ERRORS #

The **pmemset_map**() can fail with the following errors:

* **PMEMSET_E_CANNOT_ALLOCATE_INTERNAL_STRUCTURE** - an internal structure
needed by the function cannot be allocated.

* **PMEMSET_E_INVALID_OFFSET_VALUE** - the offset value assigned to the part
is invalid. Offset value is bigger than INT64_MAX.

* **PMEMSET_E_GRANULARITY_NOT_SUPPORTED** - the granularity stored in the
provided *map_cfg* defined in *set* is invalid. The concept of granularity is explained
in **libpmem2**(7) manpage.

* **PMEMSET_E_INVALID_PMEM2_MAP** - the pmem2 mapping that pmemset mapping relies on
cannot be created. The error code of **libpmem2**(7) error is printed in the logs and
can be checked for further information.

* **PMEMSET_E_INVALID_SOURCE_TYPE** - source *src* is not a valid value.

* **PMEMSET_E_LENGTH_UNALIGNED** - the length of the part to be mapped is not aligned
to the allocation granularity.

* **PMEMSET_E_CANNOT_COALESCE_PARTS** - new part couldn't be coalesced with previously
mapped part in the pmemset. The memory range after the ending address of previous
mapped part is occupied.

* **PMEMSET_E_CANNOT_GROW_SOURCE_FILE** - in case of **pmemset_source_from_temporary**(3)
or **pmemset_xsource_from_file**(3), temporary file created in *dir* cannot
be extended for the defined part size and offset.

* **-ENOMEM** in case of insufficient memory to allocate an instance
of *struct pmemset_part_map*.

* **PMEMSET_E_CANNOT_FIT_PART_MAP** - in case of pmemset created from config with a
reservation set, provided reservation has no space for a new part mapping.

* **PMEMSET_E_UNDESIRABLE_PART_STATE** - determined state of the part to be mapped does not
match any of the acceptable states set in the SDS structure. For more information please see
**pmemset_config_set_acceptable_states**(3).

* **PMEMSET_E_SDS_ENOSUPP** - device that stores the data described by the *struct pmemset_source*
does not support unsafe shutdown feature that SDS depends on. To avoid this error user should not
reference the *struct pmemset_sds* in the source using **pmemset_source_set_sds**(3) function.

* **PMEMSET_E_SDS_DEVICE_ID_LEN_TOO_BIG** - device id is too big and can't fit into the buffer
with predefined size *PMEMSET_SDS_DEVICE_ID_LEN*.

* **PMEMSET_E_MAP_LENGTH_UNSET** - the length of the map is unset and equals zero.

* **PMEMSET_E_SOURCE_FILE_IS_TOO_SMALL** - the size of the source file could not be extended and is too small
for the part to be mapped.

It can also return **libpmem2**(7) errors from the underlying functions.

# SEE ALSO #

**pmemset_config_set_acceptable_states**(3),
**pmemset_config_set_reservation**(3),**pmemset_first_part_map**(3),
**pmemset_next_part_map**(3), **pmemset_part_map_by_address**(3),
**pmemset_set_contiguous_part_coalescing**(3),
**pmemset_source_from_file**(3), **pmemset_source_from_pmem2**(3),
**pmemset_source_from_temporary**(3), **pmemset_source_set_sds**(3),
**pmemset_xsource_from_file**(3),**libpmem2**(7),
**libpmemset**(7) and **<http://pmem.io>**

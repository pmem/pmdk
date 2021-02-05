---
layout: manual
Content-Style: 'text/css'
title: _MP(PMEMSET_PART_MAP, 3)
collection: libpmemset
header: PMDK
date: pmemset API version 1.0
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2020, Intel Corporation)

[comment]: <> (pmemset_part_map.3 -- man page for libpmemset pmemset_part_map operation)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[ERRORS](#errors)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmemset_part_map**() - creates a part mapping

# SYNOPSIS #

```c
#include <libpmemset.h>

struct pmemset_extras;
struct pmemset_part;
struct pmemset_part_descriptor;
struct pmemset_source;
int pmemset_part_map(struct pmemset_part **part_ptr,
		struct pmemset_extras *extra,
		struct pmemset_part_descriptor *desc);
```

# DESCRIPTION #

The **pmemset_part_map**() function creates new part mapping in the virtual address space
of the calling process and adds structure describing this mapping to the pmemset. It requires
an address of a pointer to initialized part provided in the *part_ptr* parameter. A part can be created using
**pmemset_part_new**(3) function. The mapping can later be retrieved using **pmemset_first_part_map**(3),
**pmemset_next_part_map**(3) and **pmemset_part_map_by_address**(3) functions.

Optionally **pmemset_part_map**() function can take a part descriptor object passed via *desc* parameter.
If an optional descriptor was provided then address and size of the part mapping are stored in the
descriptor when this function succeeds.

During the lifespan of initialized pmemset, a contiguous part coalescing feature value can
be set using **pmemset_set_contiguous_part_coalescing**() function, modifying the default behavior of
part mapping. With contiguous part coalescing feature enabled, **pmemset_part_map**() function tries to map each
new part at the virtual memory region that is situated right after the previous mapped part memory range.

When the **pmemset_part_map**() function succeeds it consumes the part thereby deleting it and
the variable pointed by *part_ptr* is set to NULL.

# RETURN VALUE #

The **pmemset_part_map**() function returns 0 on success
or a negative error code on failure.

# ERRORS #

The **pmemset_part_map**() can fail with the following errors:

* **PMEMSET_E_CANNOT_ALLOCATE_INTERNAL_STRUCTURE** - an internal structure
needed by the function cannot be allocated.

* **PMEMSET_E_INVALID_OFFSET_VALUE** - the offset value assigned to the part
is invalid. Offset value is bigger than INT64_MAX.

* **PMEMSET_E_GRANULARITY_NOT_SUPPORTED** - the granularity stored in the
provided part *part_ptr* is invalid. The concept of granularity is explained
in **libpmem2**(7) manpage.

* **PMEMSET_E_INVALID_PMEM2_MAP** - the pmem2 mapping that pmemset mapping relies on
cannot be created. The error code of **libpmem2**(7) error is printed in the logs and
can be checked for further information.

* **PMEMSET_E_LENGTH_UNALIGNED** - the length of the part to be mapped is not aligned
to the allocation granularity.

* **PMEMSET_E_CANNOT_COALESCE_PARTS** - new part couldn't be coalesced with previously
mapped part in the pmemset. The memory range after the ending address of previous
mapped part is occupied.

* **PMEMSET_E_CANNOT_TRUNCATE_SOURCE_FILE** - in case of **pmemset_source_from_temporary**(3)
or **pmemset_xsource_from_file**(3) *PMEMSET_SOURCE_FILE_TRUNCATE_IF_NEEDED* flag,
temporary file created in *dir* cannot be truncated for the defined part size and offset.

* **-ENOMEM** in case of insufficient memory to allocate an instance
of *struct pmemset_part_map*.

# SEE ALSO #

**pmemset_first_part_map**(3), **pmemset_next_part_map**(3),
**pmemset_part_map_by_address**(3), **pmemset_part_new**(3),
**pmemset_set_contiguous_part_coalescing**(3),
**pmemset_source_from_temporary**(3), **pmemset_xsource_from_file**(3),
**libpmemset**(7), **libpmem2**(7),
and **<http://pmem.io>**

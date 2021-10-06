---
layout: manual
Content-Style: 'text/css'
title: _MP(PMEM2_MAP_NEW, 3)
collection: libpmem2
header: PMDK
date: pmem2 API version 1.0
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2019-2021, Intel Corporation)

[comment]: <> (pmem2_map_new.3 -- man page for libpmem2 pmem2_map_new operation)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[ERRORS](#errors)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmem2_map_new**() - creates a mapping

# SYNOPSIS #

```c
#include <libpmem2.h>

struct pmem2_config;
struct pmem2_source;
struct pmem2_map;
int pmem2_map_new(struct pmem2_map **map_ptr, const struct pmem2_config *config,
		const struct pmem2_source *source);
```

# DESCRIPTION #

The **pmem2_map_new**() function creates a new mapping in the virtual address space
of the calling process. This function requires a configuration
*config* of the mapping and the data source *source*.

Optionally, the mapping can be created at the offset of the virtual memory reservation
set in the configuration *config*. See **pmem2_config_set_vm_reservation**(3) for details.

For a mapping to succeed, the *config* structure must have the granularity
parameter set to the appropriate level. See **pmem2_config_set_required_store_granularity**(3)
and **libpmem2**(7) for more details.

If the **pmem2_map_new**() function succeeds in creating a new mapping it
instantiates a new *struct pmem2_map** object describing the mapping. The
pointer to this newly created object is stored in the user-provided variable
passed via the *map_ptr* pointer. If the mapping fails the variable pointed by
*map_ptr* will contain a NULL value and appropriate error value will be
returned. For a list of possible return values please see
[RETURN VALUE](#return-value).

All *struct pmem2_map* objects created via the **pmem2_map_new**() function have to
be destroyed using the **pmem2_map_delete**() function. For details please see
**pmem2_map_delete**(3) manual page.

# RETURN VALUE #

The **pmem2_map_new**() function returns 0 on success
or a negative error code on failure.

# ERRORS #

The **pmem2_map_new**() can fail with the following errors:

* **PMEM2_E_GRANULARITY_NOT_SET** - the store granularity for the mapping was
not set in the provided *config* structure. Please see **pmem2_config_set_required_store_granularity**(3)
and **libpmem2**(7).

* **PMEM2_E_MAP_RANGE** - *offset* + *length* is too big to represent it using
*size_t* data type

* **PMEM2_E_MAP_RANGE** - end of the mapping (*offset* + *length*) is outside
of the file. The file is too small.

* **PMEM2_E_SOURCE_EMPTY** - mapped file has size equal to 0.

* **PMEM2_E_MAPPING_EXISTS** - if the object exists before the function call.
For details please see **CreateFileMapping**() manual pages. (Windows only)

* **PMEM2_E_OFFSET_UNALIGNED** - argument unaligned, offset is not a multiple of
the alignment required for specific *\*source*. Please see
**pmem2_source_alignement**(3).

* **PMEM2_E_LENGTH_UNALIGNED** - argument unaligned, length is not a multiple of
the alignment required for specific *\*source*. Please see
**pmem2_source_alignement**(3).

* **PMEM2_E_SRC_DEVDAX_PRIVATE** - device DAX mapped with MAP_PRIVATE. (Linux only)

* **PMEM2_E_ADDRESS_UNALIGNED** - when mapping device DAX to a virtual memory reservation
and the base mapping address (reservation address + reservation offset) is not aligned
to the device DAX granularity. Please see **pmem2_config_set_vm_reservation**(3). (Linux only)

* **PMEM2_E_ADDRESS_UNALIGNED** - when mapping to a virtual memory reservation and the region
for the mapping exceeds reservation size. Please see **pmem2_config_set_vm_reservation**(3).

* **PMEM2_E_NOSUPP** - when config-provided protection flags combination is not supported.

* **PMEM2_E_NO_ACCESS** - there is a conflict between mapping protection and file opening mode.

It can also return **-EACCES**, **-EAGAIN**, **-EBADF**, **-ENFILE**,
**-ENODEV**, **-ENOMEM**, **-EPERM**, **-ETXTBSY** from the underlying
**mmap**(2) function. It is used with and without **MAP_ANONYMOUS**.

**-EACCES** may be returned only if the file descriptor points to an
append-only file.

It can also return all errors from the underlying
**pmem2_source_size**() and **pmem2_source_alignment**() functions.

# SEE ALSO #

**mmap**(2), **open**(3),
**pmem2_config_set_required_store_granularity**(3),
**pmem2_source_alignment**(3), **pmem2_source_from_fd**(3),
**pmem2_source_size**(3), **pmem2_map_delete**(3),
**pmem2_config_set_vm_reservation**(3),
**libpmem2**(7) and **<https://pmem.io>**

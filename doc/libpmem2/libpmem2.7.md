---
layout: manual
Content-Style: 'text/css'
title: _MP(LIBPMEM2, 7)
collection: libpmem2
header: PMDK
date: pmem2 API version 1.0
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause
[comment]: <> (Copyright 2019-2020, Intel Corporation)

[comment]: <> (libpmem2.7 -- man page for libpmem2)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[GRANULARITY](#granularity)<br />
[CAVEATS](#caveats)<br />
[ENVIRONMENT](#environment)<br />
[DEBUGGING](#debugging)<br />
[EXAMPLE](#example)<br />
[ACKNOWLEDGEMENTS](#acknowledgements)<br />
[SEE ALSO](#see-also)

# NAME #

**libpmem2** - persistent memory support library

# SYNOPSIS #

```c
#include <libpmem2.h>
cc ... -lpmem2
```

# DESCRIPTION #

**libpmem2** provides low-level *persistent memory* (pmem) support for
applications using direct access storage (DAX), which is storage that
supports load/store access without paging blocks from a block storage
device. Some types of *non-volatile memory DIMMs* (NVDIMMs) provide this
type of byte addressable access to storage. A *persistent memory aware
file system* is typically used to expose the direct access to
applications. Memory mapping a file from this type of file system
results in the load/store, non-paged access to pmem.

This library is for applications that use persistent memory directly,
without the help of any library-supplied transactions or memory
allocation. Higher-level libraries that *currently* build on **libpmem**
(previous variation of libpmem2) are available and are recommended for
most applications, see:

+ **libpmemobj**(7), a general use persistent memory API, providing memory
  allocation and transactional operations on variable-sized objects.

+ **libpmemblk**(7), providing pmem-resident arrays of fixed-sized blocks
  with atomic updates.

+ **libpmemlog**(7), providing a pmem-resident log file.

The **libpmem2** library provides a comprehensive set of functions for
robust use of Persistent Memory. It relies on three core concepts:
*struct pmem2_src source*, *struct pmem2_config config* and *struct pmem2_map map*:

* *source* - an object describing the data source for mapping.
The data source can be a file descriptor, a file handle, or an anonymous mapping.
APIs dedicated for creating *source* are: **pmem2_source_from_fd**(3),
 **pmem2_source_from_handle**(3), **pmem2_source_from_anon**(3).

* *config* - an object containing parameters that are used to create a mapping from a *source*.
The configuration structure must always be provided to create a mapping,
but the only required parameter to set in the *config* is *granularity*.
The granularity should by set using dedicated **libpmem2** function
**pmem2_config_set_required_store_granularity**(3) which defines a maximum permitted
granularity requested by the user. For more information about the granularity concept
read **GRANULARITY** section below.

In addition to the granularity setting, libpmem2 provides multiple optional
functions to configure target mapping, e.g., **pmem2_config_set_length**(3)
to set length which will be used for mapping, or **pmem2_config_set_offset**(3)
which will be used to map the contents from the specified location of the source,
**pmem2_config_set_sharing**(3) which defines the behavior and visibility of writes
to the mapping's pages.

* *map* - an object created by **pmem2_map_new**(3) using *source* and
*config* as an input parameters. The map structure can be then used to
directly operate on the created mapping through the use of its associated
set of functions: **pmem2_map_get_address**(3), **pmem2_map_get_size**(3),
**pmem2_map_get_store_granularity**(3) - for getting address,
size and effective mapping granularity.

In addition to the basic functionality of managing the virtual address mapping,
**libpmem2** also provides optimized functions for modifying the mapped data.
This includes data flushing as well as memory copying.

To get proper function for data flushing use: **pmem2_get_flush_fn**(3),
**pmem2_get_persist_fn**(3) or **pmem2_get_drain_fn**(3).
To get proper function for copying to persistent memory, use *map* getters:
**pmem2_get_memcpy_fn**(3), **pmem2_get_memset_fn**(3), **pmem2_get_memmove_fn**(3).

The **libpmem2** API also provides support for the badblock and unsafe shutdown
state handling.

To read or clear badblocks, the following functions are provided:
**pmem2_badblock_context_new**(3), **pmem2_badblock_context_delete**(3),
**pmem2_badblock_next**(3) and **pmem2_badblock_clear**(3).

To handle unsafe shutdown in the application, the following functions are provided:
**pmem2_source_device_id**(3), **pmem2_source_device_usc**(3).
More detailed information about unsafe shutdown detection and unsafe shutdown count
and can be found in the **libpmem2_unsafe_shutdown**(7) man page.

# GRANULARITY #

The **libpmem2** library introduces the concept of granularity through which you
may easily distinguish between different levels of storage performance
capabilities available to the application as related to *power-fail protected domain*.
The way data reaches this protected domain differs based on the platform
and storage device capabilities.

Traditional block storage devices (SSD, HDD) must use system API calls such
as `msync()`, `fsync()` on Linux, or `FlushFileBuffers()`,`FlushViewOfFile()`
on Windows to write data reliably. Invoking these functions flushes the data
to the medium with page granularity. In the **libpmem2** library, this type
of flushing behavior is called **PMEM2_GRANULARITY_PAGE**.

In systems with persistent memory support, a *power-fail protected domain* may
cover different sets of resources: either the memory controller or the memory
controller and CPU caches. For this reason, **libpmem2** distinguishes two types
of granularity for persistent memory:
**PMEM2_GRANULARITY_CACHE_LINE** and **PMEM2_GRANULARITY_BYTE**.

If the *power-fail protected domain* covers only the memory controller, the
CPU appropriate cache lines must be flushed for the data to be considered
persistent. This granularity type is called **PMEM2_GRANULARITY_CACHE_LINE**.
Depending on the architecture, there are
different types of machine instructions for flushing *cache lines*
(e.g., *CLWB*, *CLFLUSHOPT*, *CLFLUSH* for Intel x86_64 architecture). Usually,
to ensure the ordering of stores, such instructions must be followed
by a barrier (e.g., *SFENCE*).

The third type of granularity **PMEM2_GRANULARITY_BYTE** applies to platforms
where *power-fail protected domain* covers both the memory controller and
CPU caches. In such cases, cache flush instructions are no longer needed, and
the platform itself guarantees the persistence of data. But barriers might
still be required for ordering.

The library declares these granularity level in *pmem2_granularity* enum, which
the application must set in *pmem2_config* to the appropriate level for
a mapping a succeed. The software should set this config parameter to a value
that most accurately represents the target hardware characteristics and
the storage patterns of the application. For example, a database storage engine
that operates on large logical pages that reside either on SSDs or PMEM should
set this value to **PMEM2_GRANULARITY_PAGE**.
The library will create mappings where the new map granularity is lower or
equal to the requested one. For example, a mapping with **PMEM2_GRANULARITY_CACHE_LINE**
can created for the requried granularity **PMEM2_GRANULARITY_PAGE**, but not
vice versa.

# CAVEATS #

**libpmem2** relies on the library destructor being called from the main thread. For this reason, all functions that might trigger destruction (e.g. dlclose(3)) should be called in the main thread. Otherwise some of the resources associated with that thread might not be cleaned up properly.

# ENVIRONMENT #

**libpmem2** can change its default behavior based on the following
environment variables. These are primarily intended for testing and are
generally not required.

+ **PMEM2_FORCE_GRANULARITY**=*val*

Setting this environment variable to *val* forces **libpmem2** to
use persist method specific for forced granularity and skip
granularity autodetecting mechanism. The concept of the granularity is
described in *GRANULARITY* section above.
This variable is intended for use during library testing.

The *val* argument accepts following text values:

+ **BYTE** - force byte granularity.
+ **CACHE_LINE** - force cache line granularity.
+ **PAGE** - force page granularity.

Granularity values listed above are case-insensitive.

>NOTE:
The value of **PMEM2_FORCE_GRANULARITY** is not queried (and cached)
at library initialization time, but read during each **pmem2_map_new**(3) call.

This means that **PMEM2_FORCE_GRANULARITY** may still be set or modified
by the program until the first attempt to map a file.

+ **PMEM_NO_CLWB**=1

Setting this environment variable to 1 forces **libpmem2** to never issue
the **CLWB** instruction on Intel hardware, falling back to other cache
flush instructions on that hardware instead (**CLFLUSHOPT** or **CLFLUSH**).
Without this setting, **libpmem2** will always use the **CLWB** instruction
for flushing processor caches on platforms that support this instruction.
This variable is intended for use during library testing, but may be required
for some rare cases when using **CLWB** has a negative impact on performance.

+ **PMEM_NO_CLFLUSHOPT**=1

Setting this environment variable to 1 forces **libpmem2** to never issue
the **CLFLUSHOPT** instruction on Intel hardware, falling back to the
**CLFLUSH** instructions instead. Without this environment variable,
**libpmem2** will always use the **CLFLUSHOPT** instruction for flushing
processor caches on platforms that support the instruction, but where
**CLWB** is not available. This variable is intended for use during
library testing.

+ **PMEM_NO_MOVNT**=1

Setting this environment variable to 1 forces **libpmem2** to never use
the *non-temporal* move instructions on Intel hardware. Without this
environment variable, **libpmem2** will use the non-temporal instructions
for copying larger ranges to persistent memory on platforms that support
these instructions. This variable is intended for use during library
testing.

+ **PMEM_MOVNT_THRESHOLD**=*val*

This environment variable allows overriding the minimum length of
the *pmem2_memmove_fn* operations, for which **libpmem2** uses
*non-temporal* move instructions. Setting this environment variable to 0
forces **libpmem2** to always use the *non-temporal* move instructions if
available. It has no effect if **PMEM_NO_MOVNT** is set to 1.
This variable is intended for use during library testing.

# DEBUGGING #

Two versions of **libpmem2** are typically available on a development
system. The normal version, accessed when a program is linked using the
**-lpmem2** option, is optimized for performance. That version skips checks
that impact performance and never logs any trace information or performs any
run-time assertions.

A second version of **libpmem2**, accessed when a program uses the libraries
under _DEBUGLIBPATH(), contains run-time assertions and trace points. The
typical way to access the debug version is to set the environment variable
**LD_LIBRARY_PATH** to _LDLIBPATH(). Debugging output is
controlled using the following environment variables. These variables have
no effect on the non-debug version of the library.

+ **PMEM2_LOG_LEVEL**

The value of **PMEM2_LOG_LEVEL** enables trace points in the debug version
of the library, as follows:

+ **0** - This is the default level when **PMEM2_LOG_LEVEL** is not set.
No log messages are emitted at this level.

+ **1** - Additional details on any errors detected are logged, in addition
to returning the *errno*-based errors as usual. The same information
may be retrieved using _UW(pmem2_errormsg).

+ **2** - A trace of basic operations is logged.

+ **3** - Enables a very verbose amount of function call tracing in the
library.

+ **4** - Enables voluminous and fairly obscure tracing
information that is likely only useful to the **libpmem2** developers.

Unless **PMEM2_LOG_FILE** is set, debugging output is written to *stderr*.

+ **PMEM2_LOG_FILE**

Specifies the name of a file where
all logging information should be written. If the last character in the name
is "-", the *PID* of the current process will be appended to the file name when
the log file is created. If **PMEM2_LOG_FILE** is not set, output is
written to *stderr*.

# EXAMPLE #

The following example uses **libpmem2** to flush changes made to raw,
memory-mapped persistent memory.

>WARNING:
There is nothing transactional about the *persist* from **pmem2_get_persist_fn**(3)
call in this example. Interrupting the program may result in a partial write to pmem.
Use a transactional library such as **libpmemobj**(7) to avoid torn updates.

<code data-gist-id='wlemkows/c7dc06875ee4aa9857020eba01114e75' data-gist-file='basic.c' data-gist-line='8-74'  data-gist-hide-footer='true'></code>

The above example is described in detail [here](https://pmem.io/pmdk/libpmem2/).

# ACKNOWLEDGEMENTS #

**libpmem2** builds on the persistent memory programming model recommended
by the SNIA NVM Programming Technical Work Group:
<https://snia.org/nvmp>

# SEE ALSO #

**FlushFileBuffers**(), **fsync**(2), **msync**(2),
**pmem2_config_set_length**(3), **pmem2_config_set_offset**(3),
**pmem2_config_set_required_store_granularity**(3),
**pmem2_config_set_sharing**(3),**pmem2_get_drain_fn**(3),
**pmem2_get_flush_fn**(3), **pmem2_get_memcpy_fn**(3),
**pmem2_get_memmove_fn**(3), **pmem2_get_memset_fn**(3),
**pmem2_get_persist_fn**(3),**pmem2_map_get_store_granularity**(3),
**pmem2_map_new**(3), **pmem2_source_from_anon**(3),
**pmem2_source_from_fd**(3), **pmem2_source_from_handle**(3),
**libpmem2_unsafe_shutdown**(7), **libpmemblk**(7),
**libpmemlog**(7), **libpmemobj**(7) and **<https://pmem.io>**

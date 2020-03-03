---
layout: manual
Content-Style: 'text/css'
title: LIBPMEM2
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

**libpmem2** - persistent memory support library (EXPERIMENTAL)

# SYNOPSIS #

```c
#include <libpmem2.h>
cc ... -lpmem2
```

# DESCRIPTION #

# GRANULARITY #

The **libpmem2** library introduces the concept of granularity
through which you may easily distinguish between different types
of reaching *power-fail protected domain* by data. Data can reach
this domain in different ways depending on the platform capabilities.

In the case of block storage devices (SSD, HDD), data must be written
using system API calls such as `msync()`, `fsync()` on Linux
or `FlushFileBuffers()` on Windows. When these functions are invoked,
the data is flushed to the medium with page size accuracy.
In the **libpmem2** library, this type of precision is called
**PMEM2_GRANULARITY_PAGE**.

In systems with persistent memory support a *power-fail protected domain* may
cover different sets of resources: either the memory controller or the memory
controller and CPU caches. In this regard **libpmem2** distinguish two types of
granularity for persistent memory: **PMEM2_GRANULARITY_CACHE_LINE** and
**PMEM2_GRANULARITY_BYTE**.

If the *power-fail protected domain* covers only the memory controller it is
required to flush CPU caches, so the granularity type, in this case, is called
**PMEM2_GRANULARITY_CACHE_LINE**. Depending on the architecture, there are
different types of machine instructions for flushing *cache lines* (e.g. *CLWB*,
*CLFLUSHOPT*, *CLFLUSH* for Intel x86_64 architecture). Besides, each of these
instructions must be followed by a serializing operation (e.g. *SFENCE*) to ensure
the latest cache flushing was completed and the order of stores was preserved.

The third type of granularity **PMEM2_GRANULARITY_BYTE** applies to platforms
where *power-fail protected domain* covers both the memory controller and CPU caches.
This means cache flush instructions are no longer needed and the data persistence is
guaranteed by the battery installed on the platform.
Only serializing instruction is still needed to keep write sequence correct.

The **libpmem2** exposes a *pmem2_granularity* enum which can be used for
specifying granularity requested by a user and for reading what the effective
granularity for the mapping is.

For more details about *pmem2_granularity* definition and usage, see
**pmem2_config_set_required_store_granularity**(3) and
**pmem2_map_get_store_granularity**(3) manpages.

# CAVEATS #

# ENVIRONMENT #

**libpmem2** can change its default behavior based on the following
environment variables. These are largely intended for testing and are
not normally required.

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
at library initialization time, but read during each **pmem2_map**(3) call.

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
under **/pmdk/src/x64/Debug**, contains run-time assertions and trace points. The
typical way to access the debug version is to set the environment variable
**LD_LIBRARY_PATH** to **/pmdk/src/x64/Debug**. Debugging output is
controlled using the following environment variables. These variables have
no effect on the non-debug version of the library.

+ **PMEM2_LOG_LEVEL**

The value of **PMEM2_LOG_LEVEL** enables trace points in the debug version
of the library, as follows:

+ **0** - This is the default level when **PMEM2_LOG_LEVEL** is not set.
No log messages are emitted at this level.

+ **1** - Additional details on any errors detected are logged, in addition
to returning the *errno*-based errors as usual. The same information
may be retrieved using **pmem2_errormsgU**()/**pmem2_errormsgW**().

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

# ACKNOWLEDGEMENTS #

**libpmem2** builds on the persistent memory programming model recommended
by the SNIA NVM Programming Technical Work Group:
<https://snia.org/nvmp>

# SEE ALSO #

**FlushFileBuffers**(), **fsync**(2), **msync**(2),
**pmem2_config_set_required_store_granularity**(3),
**pmem2_get_memset_fn**(3), **pmem2_map_get_store_granularity**(3),
**libpmemblk**(7), **libpmemlog**(7), **libpmemobj**(7)
and **<https://pmem.io>**

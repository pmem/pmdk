---
layout: manual
Content-Style: 'text/css'
title: _MP(PMEM2\_CONFIG\_SET\_OFFSET, 3)
collection: libpmem2
header: PMDK
date: pmem2 API version 1.0
...

[comment]: <> (Copyright 2019, Intel Corporation)

[comment]: <> (Redistribution and use in source and binary forms, with or without)
[comment]: <> (modification, are permitted provided that the following conditions)
[comment]: <> (are met:)
[comment]: <> (    * Redistributions of source code must retain the above copyright)
[comment]: <> (      notice, this list of conditions and the following disclaimer.)
[comment]: <> (    * Redistributions in binary form must reproduce the above copyright)
[comment]: <> (      notice, this list of conditions and the following disclaimer in)
[comment]: <> (      the documentation and/or other materials provided with the)
[comment]: <> (      distribution.)
[comment]: <> (    * Neither the name of the copyright holder nor the names of its)
[comment]: <> (      contributors may be used to endorse or promote products derived)
[comment]: <> (      from this software without specific prior written permission.)

[comment]: <> (THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS)
[comment]: <> ("AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT)
[comment]: <> (LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR)
[comment]: <> (A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT)
[comment]: <> (OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,)
[comment]: <> (SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT)
[comment]: <> (LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,)
[comment]: <> (DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY)
[comment]: <> (THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT)
[comment]: <> ((INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE)
[comment]: <> (OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.)

[comment]: <> (pmem2_config_set_offset.3 -- man page for libpmem2 config API)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmem2_config_set_offset**() - set offset in the pmem2_config structure

# SYNOPSIS #

```c
#include <libpmem2.h>

struct pmem2_config;
int pmem2_config_set_offset(struct pmem2_config *cfg, size_t offset);
```

# DESCRIPTION #

The **pmem2_config_set_offset**() function configures the offset which will be used
to map the contents from the specified location of the source. *\*cfg* should be
already initialized, please see **pmem2_config_new**(3) for details. *\offset*
should be less than or equal to **INT64_MAX**. For Linux, *\offset* must be a multiple
of **PAGESIZE** (see **sysconf**(3)). For Windows, *\offset* must match the memory
allocation granularity of the system - the offset must be a multiple of the allocation
granularity.

# RETURN VALUE #

The **pmem2_config_set_offset**() function returns 0 on success. Otherwise, it returns
one of the following error values:

* **PMEM2_E_OFFSET_OUT_OF_RANGE** - argument out of range, offset is greater than
**INT64_MAX**

* **PMEM2_E_OFFSET_UNALIGNED** - argument unaligned, offset is not a multiple of
**PAGESIZE** (see **sysconf**(3)) for Linux or is not a multiple of the allocation
granularity for Windows

# SEE ALSO #

**libpmem2**(7), **pmem2_config_new**(3), **sysconf**(3) and **<http://pmem.io>**

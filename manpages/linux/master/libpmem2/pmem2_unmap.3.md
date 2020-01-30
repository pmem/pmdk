---
layout: manual
Content-Style: 'text/css'
title: PMEM2_UNMAP
collection: libpmem2
header: PMDK
date: pmem2 API version 1.0
...

[comment]: <> (Copyright 2019-2020, Intel Corporation)

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

[comment]: <> (pmem2_unmap.3 -- man page for libpmem2 pmem2_unmap operation)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmem2_unmap**() - deletes a mapping

# SYNOPSIS #

```c
#include <libpmem2.h>

int pmem2_unmap(struct pmem2_map **map_ptr);
```

# DESCRIPTION #

The **pmem2_unmap**() function deletes the mapping described by the
*struct pmem2_map* object.

If **pmem2_unmap**() succeeds, deleting the mapping, it releases the
*struct pmem2_map* object describing it and writes a NULL value to *map_ptr*.
If the function fails, the *map_ptr* variable and the map object itself are left
unmodified and appropriate error value will be returned. For a list of possible
return values please see [RETURN VALUE](#return-value).

# RETURN VALUE #

When **pmem2_unmap**() succeeds it returns 0. Otherwise, it returns
one of the following error values:

* **PMEM2_E_MAPPING_NOT_FOUND** - mapping was not found (it was already
unmapped or pmem2_map state was corrupted)

On systems other than Windows it can also return **-EINVAL** from the underlying
**munmap**(2) function.

# SEE ALSO #

**pmem2_map(3)**, **libpmem2**(7) and **<http://pmem.io>**

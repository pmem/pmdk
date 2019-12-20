---
layout: manual
Content-Style: 'text/css'
title: _MP(PMEM2\_CONFIG\_NEW, 3)
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

[comment]: <> (pmem2_config_new.3 -- man page for pmem2_config_new and pmem2_config_delete)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmem2_config_new**(), **pmem2_config_delete**() - allocate and free a
configuration for a libpmem2 mapping

# SYNOPSIS #

```c
#include <libpmem2.h>

struct pmem2_config;
int pmem2_config_new(struct pmem2_config **config);
int pmem2_config_delete(struct pmem2_config **config);
```

# DESCRIPTION #

The **pmem2_config_new**() function instantiates a new (opaque) configuration structure, *pmem2_config*, which is used to define mapping parameters for a **pmem2_map**() function, and returns it through the pointer in *\*config*.

New configuration is always initialized with default values for all possible parameters, which are specified alongside the corresponding setter function.

The **pmem2_config_delete**() function frees *\*config* returned by **pmem2_config_new**() and sets *\*config* to NULL.

# RETURN VALUE #

The **pmem2_config_new**() function returns 0 on success or a negative error code on failure.
**pmem2_config_new**() does set *\*config* to NULL on failure.

The **pmem2_config_delete**() function returns 0.

Please see **libpmem2**(7) for detailed description of libpmem2 error codes.

# ERRORS #

**pmem2_config_new**() can fail with the following error:
- **-ENOMEM** - out of memory

# SEE ALSO #

**errno**(3), **pmem2_map**(3), **pmem2_config_set_handle**(3),
**pmem2_config_set_fd**(3), **pmem2_config_get_file_size**(3),
**libpmem2**(7) and **<http://pmem.io>**

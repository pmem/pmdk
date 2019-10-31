---
layout: manual
Content-Style: 'text/css'
title: _MP(PMEM2_CONFIG_SET_FD, 3)
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

[comment]: <> (pmem2_config_set_fd.3 -- man page for pmem2_config_set_fd

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmem2_config_set_fd**() - set a file descriptor in pmem2_config

# SYNOPSIS #

```c
#include <libpmem2.h>

int pmem2_config_set_fd(struct pmem2_config *cfg, int fd);
_WINUX(=q=int pmem2_config_set_handle(struct pmem2_config *cfg, HANDLE handle);=e=)
```

# DESCRIPTION #

The **pmem2_config_set_fd**() function _WINUX(=q=converts a file descriptor to the HANDLE by using **_get_osfhandle**() function, and passes it to **pmem2_config_set_handle**()=e=,=q=sets a file descriptor in pmem2_config=e=)

*fd* must be open with *O_RDONLY* or *O_RDRW* mode.

_WINUX(=q=The **pmem2_config_set_handle**() function sets a file handle in pmem2_config=e=)

# RETURN VALUE #

The **pmem2_config_set_fd**() _WINUX(=q=and **pmem2_config_set_handle**() functions=e=, function) returns 0 on success or one of error values listed in the next section.

# ERRORS #

- **-EBADF** *fd* is not an open file descriptor.
_WINUX(,=q=- **PMEM2_E_INVALID_FILE_HANDLE** *fd* is oppened in O_WRONLY mode=E=)

# SEE ALSO #
**errno**(3), **pmem2_map**(3), **libpmem2**(7)
and **<http://pmem.io>**

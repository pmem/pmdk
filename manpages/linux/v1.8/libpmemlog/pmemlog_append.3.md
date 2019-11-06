---
layout: manual
Content-Style: 'text/css'
title: PMEMLOG_APPEND
collection: libpmemlog
header: PMDK
date: pmemlog API version 1.1
...

[comment]: <> (Copyright 2017-2018, Intel Corporation)

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

[comment]: <> (pmemlog_append.3 -- man page for pmemlog_append and pmemlog_appendv functions)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[NOTES](#notes)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmemlog_append**(), **pmemlog_appendv**() - append bytes to the persistent
memory resident log file

# SYNOPSIS #

```c
#include <libpmemlog.h>

int pmemlog_append(PMEMlogpool *plp, const void *buf, size_t count);
int pmemlog_appendv(PMEMlogpool *plp, const struct iovec *iov, int iovcnt);
```

# DESCRIPTION #

The **pmemlog_append**() function appends *count* bytes from *buf*
to the current write offset in the log memory pool *plp*.
Calling this function is analogous to appending to a file.
The append is atomic and cannot be torn by a program failure or system crash.

The **pmemlog_appendv**() function appends to the log memory pool *plp* from
the scatter/gather list *iov* in a manner
similar to **writev**(2). The entire list of buffers is appended atomically,
as if the buffers in *iov* were concatenated in order.
The append is atomic and cannot be torn by a program failure or system crash.

# RETURN VALUE #

On success, **pmemlog_append**() and **pmemlog_appendv**() return 0.
On error, they return -1 and set *errno* appropriately.

# ERRORS #

**EINVAL** The vector count *iovcnt* is less than zero.

**ENOSPC** There is no room for the data in the log file.

**EROFS** The log file is open in read-only mode.

# NOTES #

Since **libpmemlog**(7) is designed as a low-latency code path,
many of the checks routinely done by the operating system for **writev**(2)
are not practical in the library's implementation of **pmemlog_appendv**().
No attempt is made to detect NULL or incorrect pointers, for example.

# SEE ALSO #

**writev**(2), **libpmemlog**(7) and **<http://pmem.io>**

---
draft: false
slider_enable: true
description: ""
disclaimer: "The contents of this web site and the associated <a href=\"https://github.com/pmem\">GitHub repositories</a> are BSD-licensed open source."
aliases: ["pmemlog_append.3.html"]
title: "libpmemlog | PMDK"
header: "pmemlog API version 1.1"
---

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2017-2023, Intel Corporation)

[comment]: <> (pmemlog_append.3 -- man page for pmemlog_append and pmemlog_appendv functions)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[NOTES](#notes)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmemlog_append**()(DEPRECATED), **pmemlog_appendv**()(DEPRECATED) - append bytes to the persistent
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

**writev**(2), **libpmemlog**(7) and **<https://pmem.io>**

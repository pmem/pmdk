---
draft: false
slider_enable: true
description: ""
disclaimer: "The contents of this web site and the associated <a href=\"https://github.com/pmem\">GitHub repositories</a> are BSD-licensed open source."
aliases: ["pmemset_errormsg.3.html"]
title: "libpmemset | PMDK"
header: "pmemset API version 1.0"
---

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2020, Intel Corporation)

[comment]: <> (pmemset_errormsg.3 -- man page for error handling in libpmemset)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />

# NAME #

_UW(pmemset_errormsg) - returns last error message

# SYNOPSIS #

```c
#include <libpmemset.h>

_UWFUNC(pmemset_errormsg, void)
```

_UNICODE()

# DESCRIPTION #

If an error is detected during the call to a **libpmemset**(7) function, the
application may retrieve an error message describing the reason of the failure
from _UW(pmemset_errormsg). The error message buffer is thread-local;
errors encountered in one thread do not affect its value in
other threads. The buffer is never cleared by any library function; its
content is significant only when the return value of the immediately preceding
call to a **libpmemset**(7) function indicated an error.
The application must not modify or free the error message string.
Subsequent calls to other library functions may modify the previous message.

# RETURN VALUE #

The _UW(pmemset_errormsg) function returns a pointer to a static buffer
containing the last error message logged for the current thread. If *errno*
was set, the error message may include a description of the corresponding
error code as returned by **strerror**(3).

# SEE ALSO #

**strerror**(3), **libpmemset**(7) and **<https://pmem.io>**

---
layout: manual
Content-Style: 'text/css'
title: PMEMSET_ERRORMSG
collection: libpmemset
header: PMDK
date: pmemset API version 1.0
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2020, Intel Corporation)

[comment]: <> (pmemset_errormsg.3 -- man page for error handling in libpmemset)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmemset_errormsgU**()/**pmemset_errormsgW**() - returns last error message

# SYNOPSIS #

```c
#include <libpmemset.h>

const char *pmemset_errormsgU(void);
const wchar_t *pmemset_errormsgW(void);
```


>NOTE: The PMDK API supports UNICODE. If the **PMDK_UTF8_API** macro is
defined, basic API functions are expanded to the UTF-8 API with postfix *U*.
Otherwise they are expanded to the UNICODE API with postfix *W*.

# DESCRIPTION #

If an error is detected during the call to a **libpmemset**(7) function, the
application may retrieve an error message describing the reason of the failure
from **pmemset_errormsgU**()/**pmemset_errormsgW**(). The error message buffer is thread-local;
errors encountered in one thread do not affect its value in
other threads. The buffer is never cleared by any library function; its
content is significant only when the return value of the immediately preceding
call to a **libpmemset**(7) function indicated an error.
The application must not modify or free the error message string.
Subsequent calls to other library functions may modify the previous message.

# RETURN VALUE #

The **pmemset_errormsgU**()/**pmemset_errormsgW**() function returns a pointer to a static buffer
containing the last error message logged for the current thread. If *errno*
was set, the error message may include a description of the corresponding
error code as returned by **strerror**(3).

# SEE ALSO #

**strerror**(3), **libpmemset**(7) and **<https://pmem.io>**

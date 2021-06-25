---
layout: manual
Content-Style: 'text/css'
title: PMEM2_PERROR
collection: libpmem2
header: PMDK
date: pmem2 API version 1.0
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2020, Intel Corporation)

[comment]: <> (pmem2_perror.3 -- man page for the error printing in libpmem2)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmem2_perrorU**()/**pmem2_perrorW**() - prints a descriptive error message to stderr

# SYNOPSIS #

```c
#include <libpmem2.h>

void pmem2_perrorU(const char *format, ...);
void pmem2_perrorW(const wchar_t *format, ...);
```


>NOTE: The PMDK API supports UNICODE. If the **PMDK_UTF8_API** macro is
defined, basic API functions are expanded to the UTF-8 API with postfix *U*.
Otherwise they are expanded to the UNICODE API with postfix *W*.

# DESCRIPTION #

The **pmem2_perrorU**()/**pmem2_perrorW**() function produces a message on standard error stream describing
the last error encountered during library call.

**pmem2_perrorU**()/**pmem2_perrorW**() takes a variable number of arguments. First, the argument string
*format* is printed - similarly to the **printf**(3), followed by a colon and a blank.
Then an error message retrieved from the **pmem2_errormsgU**()/**pmem2_errormsgW**(), and a new-line. To see
how the error message is generated, please see **pmem2_errormsg**(3).

# SEE ALSO #

**libpmem2**(7), **perror**(3), **pmem2_errormsg**(3), **printf**(3) and **<https://pmem.io>**

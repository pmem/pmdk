---
layout: manual
Content-Style: 'text/css'
title: PMEMSET_PERROR
collection: libpmemset
header: PMDK
date: pmemset API version 1.0
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2020, Intel Corporation)

[comment]: <> (pmemset_perror.3 -- man page for the error printing in libpmemset)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmemset_perrorU**()/**pmemset_perrorW**() - prints a descriptive error message to stderr

# SYNOPSIS #

```c
#include <libpmemset.h>

void pmemset_perrorU(const char *format, ...);
void pmemset_perrorW(const wchar_t *format, ...);
```


>NOTE: The PMDK API supports UNICODE. If the **PMDK_UTF8_API** macro is
defined, basic API functions are expanded to the UTF-8 API with postfix *U*.
Otherwise they are expanded to the UNICODE API with postfix *W*.

# DESCRIPTION #

The **pmemset_perrorU**()/**pmemset_perrorW**() function produces a message on standard error stream describing
the last error encountered during library call.

**pmemset_perrorU**()/**pmemset_perrorW**() takes a variable number of arguments. First, the argument string
*format* is printed - similarly to the **printf**(3), followed by a colon and a blank.
Then an error message retrieved from the **pmemset_errormsgU**()/**pmemset_errormsgW**(), and a new-line. To see
how the error message is generated, please see **pmemset_errormsg**(3).

# SEE ALSO #

**libpmemset**(7), **perror**(3), **pmemset_errormsg**(3),
**printf**(3) and **<http://pmem.io>**

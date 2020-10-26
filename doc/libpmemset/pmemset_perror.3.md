---
layout: manual
Content-Style: 'text/css'
title: _MP(PMEMSET_PERROR, 3)
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

_UW(pmemset_perror) - prints a descriptive error message to stderr

# SYNOPSIS #

```c
#include <libpmemset.h>

_UWFUNCR1(void, pmemset_perror, *format, ...)
```

_UNICODE()

# DESCRIPTION #

The _UW(pmemset_perror) function produces a message on standard error stream describing
the last error encountered during library call.

_UW(pmemset_perror) takes a variable number of arguments. First, the argument string
*format* is printed - similarly to the **printf**(3), followed by a colon and a blank.
Then an error message retrieved from the _UW(pmemset_errormsg), and a new-line. To see
how the error message is generated, please see **pmemset_errormsg**(3).

# SEE ALSO #

**libpmemset**(7), **perror**(3), **pmemset_errormsg**(3),
**printf**(3) and **<http://pmem.io>**

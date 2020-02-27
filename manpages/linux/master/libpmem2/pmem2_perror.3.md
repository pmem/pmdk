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

**pmem2_perror**() - prints a descriptive error message to stderr

# SYNOPSIS #

```c
#include <libpmem2.h>

void pmem2_perror(const char *format, ...);
```



# DESCRIPTION #

The **pmem2_perror**() function produces a message on standard error stream describing
the last error encountered during library call.

**pmem2_perror**() takes a variable number of arguments. First, the argument string
*format* is printed - similarly to the **printf**(3), followed by a colon and a blank.
Then an error message retrieved from the **pmem2_errormsg**(), and a new-line. To see
how the error message is generated, please see **pmem2_errormsg**(3).

# SEE ALSO #

**libpmem2**(7), **perror**(3), **pmem2_errormsg**(3), **printf**(3) and **<http://pmem.io>**

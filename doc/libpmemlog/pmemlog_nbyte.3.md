---
draft: false
slider_enable: true
description: ""
disclaimer: "The contents of this web site and the associated <a href=\"https://github.com/pmem\">GitHub repositories</a> are BSD-licensed open source."
aliases: ["pmemlog_nbyte.3.html"]
title: "libpmemlog | PMDK"
header: "pmemlog API version 1.1"
---

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2017-2018, Intel Corporation)

[comment]: <> (pmemlog_nbyte.3 -- man page for pmemlog_nbyte function)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmemlog_nbyte**() - checks the amount of usable space in the log pool.

# SYNOPSIS #

```c
#include <libpmemlog.h>

size_t pmemlog_nbyte(PMEMlogpool *plp);
```

# DESCRIPTION #

The **pmemlog_nbyte**() function checks the amount of usable space in the log *plp*.
This function may be used on a log to determine how much usable space is
available after **libpmemlog**(7) has added its metadata to the memory pool.

# RETURN VALUE #

The **pmemlog_nbyte**() function returns the amount of usable space in the log *plp*.

# SEE ALSO #

**libpmemlog**(7) and **<https://pmem.io>**

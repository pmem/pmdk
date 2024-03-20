---
draft: false
slider_enable: true
description: ""
disclaimer: "The contents of this web site and the associated <a href=\"https://github.com/pmem\">GitHub repositories</a> are BSD-licensed open source."
aliases: ["pmem_log_get_threshold.3.html"]
title: "libpmem | PMDK"
header: "pmem API version 1.1"
---

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2024, Intel Corporation)

[comment]: <> (pmem_log_get_threshold.3 -- get the logging threshold value)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[ERRORS](#errors)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmem_log_get_threshold** - get the logging threshold value

# SYNOPSIS #

```c
	#include <libpmem.h>

	int pmem_log_get_threshold(enum pmem_log_threshold threshold,
		enum pmem_log_level *value);
```

# DESCRIPTION #

**pmem_log_get_threshold**() gets the current *value* of the *threshold*.
See **pmem_log_set_threshold**(3) for available thresholds and values.

# RETURN VALUE #

On success, **pmem_log_get_threshold**() function returns 0 and writes into
*value* the current value of the *threshold*. On failure, it returns
a non-zero value and sets errno. In case of failure, the *value* contents is undefined.

# ERRORS #

**pmem_log_get_threshold**() can fail with the following errors:

 - EINVAL - *threshold* is not **PMEM_LOG_THRESHOLD** nor **PMEM_LOG_THRESHOLD_AUX**
 - EINVAL - *value* is NULL

# SEE ALSO #

**pmem_log_set_function**(3), **pmem_log_set_threshold**(3).

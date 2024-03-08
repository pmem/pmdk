---
draft: false
slider_enable: true
description: ""
disclaimer: "The contents of this web site and the associated <a href=\"https://github.com/pmem\">GitHub repositories</a> are BSD-licensed open source."
aliases: ["pmemobj_log_get_threshold.3.html"]
title: "libpmemobj | PMDK"
header: "pmemobj API version 2.3"
---

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2024, Intel Corporation)

[comment]: <> (pmemobj_log_get_threshold.3 -- get the logging threshold level)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[ERRORS](#errors)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmemobj_log_get_threshold** - get the logging threshold level

# SYNOPSIS #

```c
	int pmemobj_log_get_threshold(enum pmemobj_log_threshold threshold,
              enum pmemobj_log_level level);
```

# DESCRIPTION #

**pmemobj_log_get_threshold**() gets the current level of the threshold.
See **pmemobj_log_set_threshold**(3) for available thresholds and levels.

# RETURN VALUE #

**pmemobj_log_get_threshold**() function returns 0 on success or returns
a non-zero value and sets errno on failure.

# ERRORS #

**pmemobj_log_get_threshold**() can fail with the following errors:
 - EINVAL - threshold is not **CORE_LOG_THRESHOLD** nor **CORE_LOG_THRESHOLD_AUX**
 - EINVAL - *level is NULL

# SEE ALSO #

**pmemobj_log_set_function**(3), **pmemobj_log_set_threshold**(3).
